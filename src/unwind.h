#pragma once

#define NOMINMAX
#include <Windows.h>
#include <llvm/Support/CommandLine.h>

using namespace llvm;

/// RUNTIME_FUNCTION related structs
typedef enum _UNWIND_OP_CODES {
  UWOP_PUSH_NONVOL = 0, /* info == register number */
  UWOP_ALLOC_LARGE,     /* no info, alloc size in next 2 slots */
  UWOP_ALLOC_SMALL,     /* info == size of allocation / 8 - 1 */
  UWOP_SET_FPREG,       /* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
  UWOP_SAVE_NONVOL,     /* info == register number, offset in next slot */
  UWOP_SAVE_NONVOL_FAR, /* info == register number, offset in next 2 slots */
  UWOP_SAVE_XMM128 = 8, /* info == XMM reg number, offset in next slot */
  UWOP_SAVE_XMM128_FAR, /* info == XMM reg number, offset in next 2 slots */
  UWOP_PUSH_MACHFRAME   /* info == 0: no error-code, 1: error-code */
} UNWIND_CODE_OPS;

// Unwind info flags
#define UNW_FLAG_EHANDLER 0x01
#define UNW_FLAG_UHANDLER 0x02
#define UNW_FLAG_CHAININFO 0x04

// UNWIND_CODE 3 bytes structure
typedef union _UNWIND_CODE {
  struct {
    unsigned char CodeOffset;
    unsigned char UnwindOp : 4;
    unsigned char OpInfo : 4;
  };
  USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
  unsigned char Version : 3;       // + 0x00 - Unwind info structure version
  unsigned char Flags : 5;         // + 0x00 - Flags (see above)
  unsigned char SizeOfProlog;      // + 0x01
  unsigned char CountOfCodes;      // + 0x02 - Count of unwind codes
  unsigned char FrameRegister : 4; // + 0x03
  unsigned char FrameOffset : 4;   // + 0x03
  UNWIND_CODE UnwindCode[1];       // + 0x04 - Unwind code array start
} UNWIND_INFO, *PUNWIND_INFO;

#define GetBeginAddress(base, entry) (uint8_t *)(base + (entry).BeginAddress)
#define GetEndAddress(base, entry) (uint8_t *)(base + (entry).EndAddress)
#define GetUnwindInfo(base, entry)                                             \
  (UNWIND_INFO *)(base + (entry).UnwindInfoAddress)

#define GetUnwindCodeEntry(info, index) ((info)->UnwindCode[index])

#define GetLanguageSpecificDataPtr(info)                                       \
  ((PVOID)&GetUnwindCodeEntry((info), ((info)->CountOfCodes + 1) & ~1))

#define GetExceptionHandler(base, info)                                        \
  ((void *)((base) + *(PULONG)GetLanguageSpecificDataPtr(info)))


static void PrintUnwindOperations(UNWIND_INFO *UnwindInfo) {
  outs() << "Unwind operations:"
         << "\n";
  for (int i = 0; i < UnwindInfo->CountOfCodes; i++) {
    UNWIND_CODE Code = GetUnwindCodeEntry(UnwindInfo, i);
    outs() << "\t";
    switch (Code.UnwindOp) {
    case UWOP_PUSH_NONVOL: {
      outs() << "UWOP_PUSH_NONVOL: ";
      outs() << "Push a nonvolatile integer register " << (int)Code.OpInfo
             << ".\n";
      break;
    }
    case UWOP_ALLOC_LARGE: {
      outs() << "UWOP_ALLOC_LARGE: ";
      outs() << "Allocate a large-sized area on the stack"
             << "\n";
      break;
    }
    case UWOP_ALLOC_SMALL: {
      outs() << "UWOP_ALLOC_SMALL: ";
      outs() << "Allocate a small-sized area on the stack. (size = "
             << (int)Code.OpInfo * 8 + 8 << " bytes)"
             << "\n";
      break;
    }
    case UWOP_SET_FPREG: {
      outs() << "UWOP_SET_FPREG: ";
      outs() << "Establish the frame pointer register."
             << "\n";
      break;
    }
    case UWOP_SAVE_NONVOL: {
      outs() << "UWOP_SAVE_NONVOL: ";
      outs() << "Save a nonvolatile integer register " << (int)Code.OpInfo
             << " on the stack."
             << "\n";
      break;
    }
    case UWOP_SAVE_NONVOL_FAR: {
      outs() << "UWOP_SAVE_NONVOL_FAR: ";
      outs() << "Save a nonvolatile integer register on the stack with a long "
                "offset."
             << "\n";
      break;
    }
    case UWOP_SAVE_XMM128: {
      outs() << "UWOP_SAVE_XMM128: ";
      outs() << "Save all 128 bits of a nonvolatile XMM register on the stack. "
             << "\n";
      break;
    }
    case UWOP_SAVE_XMM128_FAR: {
      outs() << "UWOP_SAVE_XMM128_FAR: ";
      outs() << "Save all 128 bits of a nonvolatile XMM register on the stack "
                "with a long offset. "
             << "\n";
      break;
    }
    case UWOP_PUSH_MACHFRAME: {
      outs() << "UWOP_PUSH_MACHFRAME: ";
      outs() << "Push a machine frame."
             << "\n";
      break;
    }
    default:
      outs() << "Unknown operation."
             << "\n";
    }
  }
}