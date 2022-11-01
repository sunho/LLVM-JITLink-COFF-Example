#include "disasm.h"
#include "util.h"
#include "unwind.h"

#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include <llvm/Support/InitLLVM.h>
#include <Windows.h>

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

  static void ThrowIntercept() {
    uint8_t *RSP = (uint8_t *)_AddressOfReturnAddress();
    uint8_t *PC = *(uint8_t **)(RSP + 512 + 16); // Traverse RSP stack back
    auto It = --PluginInstance->FunctionEntries.upper_bound((uint64_t)PC);
    uint8_t *Begin = GetBeginAddress(PluginInstance->ImageBase, *It->second);
    uint8_t *End = GetEndAddress(PluginInstance->ImageBase, *It->second);
    if (PC >= End) {
      return;
    }
    outs() << "Function name that raised exception: "
           << PluginInstance->Symbols[(uint64_t)Begin] << "\n";
    PluginInstance->Disassemble(MutableArrayRef<uint8_t>(Begin, End));
  }

  void Disassemble(MutableArrayRef<uint8_t> Buf) {
    SimpleDisassembler D(J.getTargetTriple().str(), Symbols);
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

  void modifyPassConfig(MaterializationResponsibility &MR,
                        jitlink::LinkGraph &G,
                        jitlink::PassConfiguration &Config) override {
    Config.PrePrunePasses.push_back([&](jitlink::LinkGraph &G) {
      jitlink::Symbol *TISym = nullptr;
      for (auto *B : G.blocks()) {
        for (auto &E : B->edges()) {
          if (E.getTarget().getName() == "_CxxThrowException") {
            if (!TISym)
              TISym = createTrampoline(G);
            E.setTarget(*TISym);
          }
        }
      }
      return Error::success();
    });

    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      for (auto *Sym : G.defined_symbols()) {
        if (Sym->getScope() != jitlink::Scope::Default)
          continue;
        Symbols[Sym->getAddress().getValue()] = Sym->getName().str();
        if (Sym->getName() == "__ImageBase") {
          ImageBase = Sym->getAddress().getValue();
        }
      }
      return Error::success();
    });

    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      if (!Log)
        return Error::success();

      auto *Sec = G.findSectionByName(".pdata");
      if (Sec) {
        for (jitlink::Block *Block : Sec->blocks()) {
          RUNTIME_FUNCTION *Funcs =
              (RUNTIME_FUNCTION *)Block->getContent().data();
          int N = Block->getContent().size() / sizeof(RUNTIME_FUNCTION);
          for (int i = 0; i < N; i++) {
            uint8_t *Start = GetBeginAddress(ImageBase, Funcs[i]);
            uint8_t *End = GetEndAddress(ImageBase, Funcs[i]);
            UNWIND_INFO *UnwindInfo = GetUnwindInfo(ImageBase, Funcs[i]);
            FunctionEntries[(uint64_t)Start] = &Funcs[i];
            PrintLine();
            outs() << "Begin Addr: " << Start << "\n";
            outs() << "End Addr: " << End << "\n";
            outs() << "Function symbol name: " << Symbols[(uint64_t)Start]
                   << "\n";
            bool HasEH = UnwindInfo->Flags & UNW_FLAG_EHANDLER;
            outs() << (HasEH ? "This block has exception handler"
                             : "(no exception handler)")
                   << "\n";
            PrintUnwindOperations(UnwindInfo);
            outs() << "\n";
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

  bool Log = false;
  LLJIT &J;
  uint64_t ImageBase;
  std::map<uint64_t, std::string> Symbols;
  std::map<uint64_t, RUNTIME_FUNCTION *> FunctionEntries;
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
  auto Plugin = std::make_unique<ExamplePlugin>(*J);
  PluginInstance = Plugin.get();

  cast<orc::ObjectLinkingLayer>(&J->getObjLinkingLayer())
      ->addPlugin(std::move(Plugin));

  ExitOnErr(J->loadOrcRuntime("clang_rt.orc-x86_64.lib"));
  PluginInstance->Log = true;

  ExitOnErr(J->addIRModule(readIRModule("main.ll")));

  auto G = ExitOnErr(StaticLibraryDefinitionGenerator::Load(
      J->getObjLinkingLayer(), "StaticLib1.lib"));
  J->getMainJITDylib().addGenerator(std::move(G));

  auto Add1Addr = ExitOnErr(J->lookup("main"));
  ExitOnErr(J->initialize(J->getMainJITDylib()));

  int (*Add1)(void) = Add1Addr.toPtr<int(void)>();
  int Result = Add1();

  return 0;
}