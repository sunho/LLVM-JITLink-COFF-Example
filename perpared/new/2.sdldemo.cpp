#include "disasm.h"
#include "unwind.h"
#include "util.h"

#include <Windows.h>
#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include <llvm/Support/InitLLVM.h>

using namespace llvm;
using namespace llvm::orc;

ExitOnError ExitOnErr;

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

  std::vector<const char *> StaticLibs = {"SDL2.lib", "SDL2main.lib", "glew32.lib"};
  for (auto LibName : StaticLibs) {
    auto G = ExitOnErr(StaticLibraryDefinitionGenerator::Load(
        J->getObjLinkingLayer(), LibName));
    J->getMainJITDylib().addGenerator(std::move(G));
  }
  std::vector<const char *> DynamicLibs = {"SDL2.dll", "User32.dll",
                                           "Shell32.dll", "glew32.dll", "opengl32.dll"};
  JITDylib &SDLDynlib =
      J->getExecutionSession().createBareJITDylib("SDLDynlib");
  for (auto LibName : DynamicLibs) {
    auto G = ExitOnErr(DynamicLibrarySearchGenerator::Load(LibName, 0));
    SDLDynlib.addGenerator(std::move(G));
  }
  J->getMainJITDylib().addToLinkOrder(SDLDynlib);
  ExitOnErr(J->loadOrcRuntime("clang_rt.orc-x86_64.lib"));
  ExitOnErr(J->addIRModule(readIRModule("main.ll")));

  auto MainAddr = ExitOnErr(J->lookup("main"));
  ExitOnErr(J->initialize(J->getMainJITDylib()));

  int (*Main)(void) = MainAddr.toPtr<int(void)>();
  int Result = Main();

  return 0;
}