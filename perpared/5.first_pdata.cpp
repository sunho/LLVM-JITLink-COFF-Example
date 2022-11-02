
    Config.PostFixupPasses.push_back([&](jitlink::LinkGraph &G) {
      if (!Log)
        return Error::success();
      auto *Sec = G.findSectionByName(".pdata");
      for (jitlink::Block *Block : Sec->blocks()) {
        RUNTIME_FUNCTION *Funcs = (RUNTIME_FUNCTION*)Block->getContent().data();
        int N = Block->getContent().size() / sizeof(RUNTIME_FUNCTION);
        for (int i = 0; i < N; i++) {
          auto Func = Funcs[i];
          uint8_t *Begin = GetBeginAddress(ImageBase, Func);
          uint8_t *End = GetEndAddress(ImageBase, Func);
          PrintLine();
          outs() << "Begin address: " << Begin << "\n";
          outs() << "End address: " << End << "\n";
        }
      }
      return Error::success();
    });
    