#pragma once
#include <vector>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>

using namespace llvm;
using namespace orc;

/// Trampoline code write helper
static void WriteCode(std::vector<char> &CodeBuf, ArrayRef<char> Buf) {
  CodeBuf.insert(CodeBuf.end(), Buf.begin(), Buf.end());
}

static void WriteSaveRegsCode(std::vector<char> &CodeBuf) {
  static char SaveRegs[] = {
      0x55,                                     // pushq %rbp
      0x48, 0x89, 0xe5,                         // movq %rsp, %rbp
      0x48, 0x81, 0xec, 0x00, 0x02, 0x00, 0x00, // subq    $512, %rsp
      0x48, 0x89, 0x4d, 0xf0,                   // movq %rcx, -16(%rbp)
      0x48, 0x89, 0x55, 0xe8,                   // movq %rdx, -24(%rbp)
      0x48, 0x89, 0x75, 0xe0,                   // movq %rsi, -32(%rbp)
      0x48, 0x89, 0x7d, 0xd8,                   // movq %rdi, -40(%rbp)
      0x4c, 0x89, 0x45, 0xd0,                   // movq %r8, -48(%rbp)
      0x4c, 0x89, 0x4d, 0xc8,                   // movq %r9, -56(% rbp)
      0x4c, 0x89, 0x55, 0xc0,                   // movq %r10, -64(%rbp)
      0x4c, 0x89, 0x5d, 0xb8,                   // movq %r11, -72(%rbp)
      0x66, 0x0f, 0x7f, 0x45, 0x80,             // movdqa %xmm0, -128(% rbp)
      0x66, 0x0f, 0x7f, 0x8d, 0x70, 0xff, 0xff,
      0xff, // movdqa %xmm1, -144(%rbp)
      0x66, 0x0f, 0x7f, 0x95, 0x60, 0xff, 0xff,
      0xff, // movdqa %xmm2, -160(%rbp)
      0x66, 0x0f, 0x7f, 0x9d, 0x50, 0xff, 0xff,
      0xff, // movdqa %xmm3, -176(%rbp)
      0x66, 0x0f, 0x7f, 0xa5, 0x40, 0xff, 0xff,
      0xff, // movdqa %xmm4, -192(%rbp)
      0x66, 0x0f, 0x7f, 0xad, 0x30, 0xff, 0xff,
      0xff, // movdqa %xmm5, -208(%rbp)
      0x66, 0x0f, 0x7f, 0xb5, 0x20, 0xff, 0xff,
      0xff, // movdqa %xmm6, -224(%rbp)
      0x66, 0x0f, 0x7f, 0xbd, 0x10, 0xff, 0xff,
      0xff,                                    // movdqa %xmm7, -240(%rbp)
      0x48, 0x89, 0x85, 0x00, 0xff, 0xff, 0xff // movq %rax, -256(%rbp)
  };
  WriteCode(CodeBuf, ArrayRef<char>(SaveRegs));
}

static void WriteCallFuncCode(std::vector<char> &CodeBuf) {
  static char CallFunc[] = {0xe8, 0x00, 0x00, 0x00, 0x00}; // callq 0x0
  WriteCode(CodeBuf, ArrayRef<char>(CallFunc));
}

static void WriteRestoreRegsCode(std::vector<char> &CodeBuf) {
  static char RestoreRegs[] = {
      0x48, 0x8b, 0x4d, 0xf0,       // movq -16(%rbp), %rcx
      0x48, 0x8b, 0x55, 0xe8,       // movq -24(%rbp), %rdx
      0x48, 0x8b, 0x75, 0xe0,       // movq -32(%rbp), %rsi
      0x48, 0x8b, 0x7d, 0xd8,       // movq -40(%rbp), %rdi
      0x4c, 0x8b, 0x45, 0xd0,       // movq -48(%rbp), %r8
      0x4c, 0x8b, 0x4d, 0xc8,       // movq -56(%rbp), %r9
      0x4c, 0x8b, 0x55, 0xc0,       // movq -64(%rbp), %r10
      0x4c, 0x8b, 0x5d, 0xb8,       // movq -72(%rbp), %r11
      0x66, 0x0f, 0x6f, 0x45, 0x80, // movdqa -128(%rbp), %xmm0
      0x66, 0x0f, 0x6f, 0x8d, 0x70, 0xff, 0xff,
      0xff, // movdqa -144(%rbp), %xmm1
      0x66, 0x0f, 0x6f, 0x95, 0x60, 0xff, 0xff,
      0xff, // movdqa -160(%rbp), %xmm2
      0x66, 0x0f, 0x6f, 0x9d, 0x50, 0xff, 0xff,
      0xff, // movdqa -176(%rbp), %xmm3
      0x66, 0x0f, 0x6f, 0xa5, 0x40, 0xff, 0xff,
      0xff, // movdqa -192(%rbp), %xmm4
      0x66, 0x0f, 0x6f, 0xad, 0x30, 0xff, 0xff,
      0xff, // movdqa -208(%rbp), %xmm5
      0x66, 0x0f, 0x6f, 0xb5, 0x20, 0xff, 0xff,
      0xff, // movdqa -224(%rbp), %xmm6
      0x66, 0x0f, 0x6f, 0xbd, 0x10, 0xff, 0xff,
      0xff,                                     // movdqa -240(%rbp), %xmm7
      0x48, 0x8b, 0x85, 0x00, 0xff, 0xff, 0xff, // movq -256(%rbp), %rax
      0x48, 0x81, 0xc4, 0x00, 0x02, 0x00, 0x00, // addq $512, %rsp
      0x5d                                      // popq %rbp
  };
  WriteCode(CodeBuf, ArrayRef<char>(RestoreRegs));
}

static void WriteJmpFuncCode(std::vector<char> &CodeBuf) {
  static char JmpFunc[] = {0xe9, 0x00, 0x00, 0x00, 0x00}; // jmp 0x0
  WriteCode(CodeBuf, ArrayRef<char>(JmpFunc));
}

/// Create a JD that has an absolute symbol and add it to link order of given JD
static Error DefineAbsoluteSymbol(JITDylib &JD, StringRef FuncName,
                                  void *Func) {
  auto &JJD = JD.getExecutionSession().createBareJITDylib(FuncName.data());
  auto Interned = JD.getExecutionSession().intern(FuncName);
  auto Symbol = JITEvaluatedSymbol((uint64_t)Func, JITSymbolFlags::Exported |
                                                      JITSymbolFlags::Callable);
  if (auto Err = JJD.define(absoluteSymbols({{Interned, Symbol}})))
    return Err;
  JD.addToLinkOrder(JJD);
  return Error::success();
}

static ThreadSafeModule readIRModule(const char *Filename) {
  auto Context = std::make_unique<LLVMContext>();
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(Filename, Err, *Context);
  return ThreadSafeModule(std::move(M), std::move(Context));
}

static void PrintLine() { outs() << "--------------------------------\n"; }

