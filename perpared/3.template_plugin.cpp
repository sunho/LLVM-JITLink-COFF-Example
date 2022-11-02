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
  }

  void modifyPassConfig(MaterializationResponsibility &MR,
                        jitlink::LinkGraph &G,
                        jitlink::PassConfiguration &Config) override {
    Config.PrePrunePasses.push_back([&](jitlink::LinkGraph &G) {
      G.dump(outs());
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