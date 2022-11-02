  auto G = ExitOnErr(StaticLibraryDefinitionGenerator::Load(J->getObjLinkingLayer(), "StaticLib1.lib"));
  J->getMainJITDylib().addGenerator(std::move(G));
