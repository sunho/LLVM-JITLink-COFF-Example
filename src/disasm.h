#pragma once
#include <llvm-c/Disassembler.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <map>

using namespace llvm;

/// Disassembler utility class that disassembles a buffer with symbol name
struct SimpleDisassembler {
  std::map<uint64_t, std::string> Symbols;
  std::string Target;

  SimpleDisassembler(StringRef Target, std::map<uint64_t, std::string> Symbols)
      : Target(Target.str()), Symbols(Symbols) {}

  void Disasm(MutableArrayRef<uint8_t> Buf) {
    LLVMDisasmContextRef D = LLVMCreateDisasm(Target.c_str(), this, 0, nullptr,
                                              SymbolizerSymbolLookUp);
    char outline[1024];
    int pos;

    pos = 0;
    while (pos < Buf.size()) {
      size_t l = LLVMDisasmInstruction(D, Buf.data() + pos, Buf.size() - pos,
                                       (uint64_t)(Buf.data() + pos), outline,
                                       sizeof(outline));
      if (!l) {
        pprint(pos, Buf.data() + pos, 1, "\t???");
        pos++;
      } else {
        pprint(pos, Buf.data() + pos, l, outline);
        pos += l;
      }
    }

    LLVMDisasmDispose(D);
  }

  static void pprint(int pos, unsigned char *buf, int len, const char *disasm) {
    int i;
    printf("%04x:  ", pos);
    for (i = 0; i < 8; i++) {
      if (i < len) {
        printf("%02x ", buf[i]);
      } else {
        printf("   ");
      }
    }

    printf("   %s\n", disasm);
  }

  static const char *SymbolizerSymbolLookUp(void *DisInfo,
                                            uint64_t ReferenceValue,
                                            uint64_t *ReferenceType,
                                            uint64_t ReferencePC,
                                            const char **ReferenceName) {
    auto *Instance = (SimpleDisassembler *)DisInfo;

    if (*ReferenceType == LLVMDisassembler_ReferenceType_In_Branch) {

      *ReferenceType = *ReferenceType =
          LLVMDisassembler_ReferenceType_InOut_None;

      *ReferenceName = nullptr;
      auto It = Instance->Symbols.find(ReferenceValue);
      if (It == Instance->Symbols.end()) {
        return nullptr;
      }
      return It->second.data();
    }

    *ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;
    return nullptr;
  }
};