#include "disasm.h"
#include "unwind.h"
#include "util.h"

#include <Windows.h>
#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include <llvm/Support/InitLLVM.h>

using namespace llvm;
using namespace llvm::orc;

ExitOnError ExitOnErr;

class ExamplePlugin;
ExamplePlugin *PluginInstance = nullptr;

class ExamplePlugin : public ObjectLinkingLayer::Plugin {
public:
  ExamplePlugin(LLJIT &J) : J(J) {
    // Make ThrowIntercept function available within JIT session
    ExitOnErr(DefineAbsoluteSymbol(J.getMainJITDylib(), "ThrowIntercept",
                                   ThrowIntercept));
  }

  void Disassemble(MutableArrayRef<uint8_t> Buf) {
    SimpleDisassembler D(J.getTargetTriple().str(), AddrToSymbolName);
    D.Disasm(Buf);
  }

  // Create trampoline JITLink block within given LinkGraph
  jitlink::Symbol *createTrampoline(jitlink::LinkGraph &G) {
    std::vector<jitlink::Edge> Edges;
    std::vector<char> CodeBuf;

    // Write x86 assembly code to CodeBuf
    WriteSaveRegsCode(CodeBuf);
    WriteCallFuncCode(CodeBuf);

    // Add relocation edge to ThrowIntercept
    auto ThrowInterceptSymbol =
        &G.addExternalSymbol("ThrowIntercept", 0, jitlink::Linkage::Strong);
    Edges.push_back(jitlink::Edge(jitlink::x86_64::PCRel32, CodeBuf.size() - 4,
                                  *ThrowInterceptSymbol, 0));

    // Write x86 assembly code to CodeBuf
    WriteRestoreRegsCode(CodeBuf);
    WriteJmpFuncCode(CodeBuf);

    // Add relocation edge to CxxThrow symbol
    auto CxxThrowSymbol = &G.addExternalSymbol(
        "__orc_rt_coff_cxx_throw_exception", 0, jitlink::Linkage::Strong);
    Edges.push_back(jitlink::Edge(jitlink::x86_64::PCRel32, CodeBuf.size() - 4,
                                  *CxxThrowSymbol, 0));

    // Create JITLink block using CodeBuf
    auto &Sec =
        G.createSection("ThrowInterceptSection",
                        jitlink::MemProt::Exec | jitlink::MemProt::Read);
    auto &B = G.createContentBlock(Sec, ArrayRef<char>(CodeBuf),
                                   ExecutorAddr(0), 64, 0);
    for (auto E : Edges)
      B.addEdge(E);
    return &G.addDefinedSymbol(B, 0, "ThrowInterceptEntry", 0,
                               jitlink::Linkage::Strong, jitlink::Scope::Local,
                               true, true);
  }

  static void ThrowIntercept() {
    uint8_t *RSP = (uint8_t *)_AddressOfReturnAddress();
    uint8_t *PC = *(uint8_t **)(RSP + 512 + 16); // Traverse RSP stack back
    auto It = --PluginInstance->AddrToFuncEntry.upper_bound((uint64_t)PC);
    RUNTIME_FUNCTION Func = It->second;
    uint8_t *Begin = GetBeginAddress(PluginInstance->ImageBase, Func);
    uint8_t *End = GetEndAddress(PluginInstance->ImageBase, Func);
    if (PC >= End)
      return;
    outs() << "Throw intercepted!\n";
    outs() << "Name of function that raised exception: "
           << PluginInstance->AddrToSymbolName[(uint64_t)Begin] << "\n";
    outs() << "Disassembly:\n";
    PluginInstance->Disassemble(MutableArrayRef<uint8_t>(Begin, End));
  }

  void modifyPassConfig(MaterializationResponsibility &MR,
                        jitlink::LinkGraph &G,
                        jitlink::PassConfiguration &Config) override {
    Config.PrePrunePasses.push_back([&](jitlink::LinkGraph &G) {
      jitlink::Symbol *Trampoline = nullptr;
      for (jitlink::Block *Block : G.blocks()) {
        for (jitlink::Edge &Edge : Block->edges()) {
          if (Edge.getTarget().getName() == "_CxxThrowException") {
            if (!Trampoline)
              Trampoline = createTrampoline(G);
            Edge.setTarget(*Trampoline);
          }
        }
      }
      return Error::success();
    });

    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      for (auto *S : G.defined_symbols()) {
        if (S->getScope() == jitlink::Scope::Default)
          AddrToSymbolName[S->getAddress().getValue()] = S->getName();
      }
      return Error::success();
    });

    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      for (auto *S : G.defined_symbols()) {
        if (S->getName() == "__ImageBase") {
          ImageBase = S->getAddress().getValue();
        }
      }
      return Error::success();
    });

    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      auto *Sec = G.findSectionByName(".pdata");
      if (Sec) {
        for (auto *B : Sec->blocks()) {
          RUNTIME_FUNCTION *Funcs = (RUNTIME_FUNCTION *)B->getContent().data();
          int N = B->getContent().size() / sizeof(RUNTIME_FUNCTION);
          for (int i = 0; i < N; i++) {
            auto Func = Funcs[i];
            uint8_t *Begin = GetBeginAddress(ImageBase, Func);
            AddrToFuncEntry[(uint64_t)Begin] = Func;
          }
        }
      }

      return Error::success();
    });
  }

  SyntheticSymbolDependenciesMap
  getSyntheticSymbolDependencies(MaterializationResponsibility &MR) override {
    return SyntheticSymbolDependenciesMap();
  }

  Error notifyFailed(MaterializationResponsibility &MR) override {
    return Error::success();
  }

  Error notifyRemovingResources(ResourceKey K) override {
    return Error::success();
  }

  void notifyTransferringResources(ResourceKey DstKey,
                                   ResourceKey SrcKey) override {}

  LLJIT &J;
  uint64_t ImageBase;
  std::map<uint64_t, std::string> AddrToSymbolName;
  std::map<uint64_t, RUNTIME_FUNCTION> AddrToFuncEntry;
};

int main(int argc, char *argv[]) {
  // Initialize LLVM and ORC.
  InitLLVM X(argc, argv);
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  LLVMInitializeX86Disassembler();
  auto Builder = LLJITBuilder();

  // Enable JITLink
  Builder.setExecutorProcessControl(
      ExitOnErr(orc::SelfExecutorProcessControl::Create(
          std::make_shared<orc::SymbolStringPool>())));
  Builder.JTMB = ExitOnErr(JITTargetMachineBuilder::detectHost());
  Builder.JTMB->setRelocationModel(Reloc::PIC_);
  Builder.JTMB->setCodeModel(CodeModel::Small);
  Builder.setObjectLinkingLayerCreator(
      [](orc::ExecutionSession &ES, const Triple &TT) {
        return std::make_unique<orc::ObjectLinkingLayer>(
            ES, ES.getExecutorProcessControl().getMemMgr());
      });

  // Load Orc runtime
  Builder.setPlatformSetUp(setUpOrcPlatform);
  auto J = ExitOnErr(Builder.create());

  // Register plugin
  auto Plugin = std::make_unique<ExamplePlugin>(*J);
  PluginInstance = Plugin.get();
  cast<orc::ObjectLinkingLayer>(&J->getObjLinkingLayer())
      ->addPlugin(std::move(Plugin));

  ExitOnErr(J->loadOrcRuntime("clang_rt.orc-x86_64.lib"));
  ExitOnErr(J->addIRModule(readIRModule("main.ll")));

  auto Add1Addr = ExitOnErr(J->lookup("main"));
  ExitOnErr(J->initialize(J->getMainJITDylib()));

  int (*Add1)(void) = Add1Addr.toPtr<int(void)>();
  int Result = Add1();

  return 0;
}