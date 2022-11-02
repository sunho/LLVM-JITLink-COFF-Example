Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      if (!Log)
        return Error::success();
      auto *Sec = G.findSectionByName(".pdata");
      if (!Sec)
        return Error::success();
      for (jitlink::Block *Block : Sec->blocks()) {
        RUNTIME_FUNCTION *Funcs = (RUNTIME_FUNCTION*)Block->getContent().data();
        int N = Block->getContent().size() / sizeof(RUNTIME_FUNCTION);
        for (int i = 0; i < N; i++) {
          auto Func = Funcs[i];
          uint8_t *Begin = GetBeginAddress(ImageBase, Func);
          uint8_t *End = GetEndAddress(ImageBase, Func);
          UNWIND_INFO *UnwindInfo = GetUnwindInfo(ImageBase, Func);
          PrintLine();
          outs() << "Function name: " << AddrToSymbolName[(uint64_t)Begin]
                 << "\n";
          outs() << "Begin address: " << Begin << "\n";
          outs() << "End address: " << End << "\n";
          if (UnwindInfo->Flags & UNW_FLAG_EHANDLER) {
            outs() << "This function has exception handler\n";
          } else {
            outs() << "(no excpetion handler)\n";
          }
          PrintUnwindOperations(UnwindInfo);
        }
      }
      return Error::success();
    });