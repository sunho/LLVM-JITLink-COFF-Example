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

  ExitOnErr(J->addIRModule(readIRModule("main.ll")));

  auto MainAddr = ExitOnErr(J->lookup("main"));
  ExitOnErr(J->initialize(J->getMainJITDylib()));

  int (*Main)(void) = MainAddr.toPtr<int(void)>();
  int Result = Main();

  return 0;
}