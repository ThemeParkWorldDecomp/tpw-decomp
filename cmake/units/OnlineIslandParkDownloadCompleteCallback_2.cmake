# Unit v51046: nine small lobby, interface and ESP sprite functions (src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp).
# The callees outside the unit are recording fakes in tests/unit_OnlineIslandParkDownloadCompleteCallback_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_OnlineIslandParkDownloadCompleteCallback_2 src/game/HLSprite.cpp src/game/online/lobby_globe.cpp
    src/game/online/lobby_island.cpp src/game/online/lobby_parser.cpp src/game/ui/intrface.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineIslandParkDownloadCompleteCallback_2_KEEP
  __Z20HLSpriteListCtorGluePv   # HLSpriteListCtorGlue(void*)
  __Z20HLSpriteListDtorGluePv   # HLSpriteListDtorGlue(void*)
  __Z20__sinit_HLSprite_cppv   # __sinit_HLSprite_cpp()
  __Z23ESP_GetRandomActionFrom11ESP_Spritesi   # ESP_GetRandomActionFrom(ESP_Sprites, int)
  __Z24__sinit_lobby_parser_cppv   # __sinit_lobby_parser_cpp()
  __Z37PostGetTop10ParksListCallbackFunctioniPKvP9TbPtrListI13CFSOnlineParkE   # PostGetTop10ParksListCallbackFunction(int, void const*, TbPtrList<CFSOnlinePark>*)
  __Z40OnlineIslandParkDownloadCompleteCallbackiPKv   # OnlineIslandParkDownloadCompleteCallback(int, void const*)
  __ZN25CLBScreenComponentIslands20ProcessCancelIslandsEv   # CLBScreenComponentIslands::ProcessCancelIslands()
  __ZN31CLBScreenComponentDownloadGlobe13ProcessCancelEv   # CLBScreenComponentDownloadGlobe::ProcessCancel()
  __ZN31CLBScreenComponentDownloadGlobe4ShowEv   # CLBScreenComponentDownloadGlobe::Show()
  __ZN9CIntrface14DrawFPSCounterEf   # CIntrface::DrawFPSCounter(float)
  _gESPFileInfoDtor
  _gESPFileInfoNode
  _gESPGlobalListNode
  _gHLSpriteArrayDtor
  _gHLSpriteArrayNode
  _gLobbyDescriptorFileNode
  _gLobbyFolderNode
  _gLobbyParserWide
  _gLobbyParserWideNode
  TPW_GLOBE_SHOWHIDE_REAL@game/HLSprite.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/HLSprite.cpp (a guard macro; src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp had it)
  TPW_GLOBE_SHOWHIDE_REAL@game/online/lobby_globe.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/online/lobby_globe.cpp (a guard macro; src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp had it)
  TPW_GLOBE_SHOWHIDE_REAL@game/online/lobby_island.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/online/lobby_island.cpp (a guard macro; src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp had it)
  TPW_GLOBE_SHOWHIDE_REAL@game/online/lobby_parser.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/online/lobby_parser.cpp (a guard macro; src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp had it)
  TPW_GLOBE_SHOWHIDE_REAL@game/ui/intrface.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/ui/intrface.cpp (a guard macro; src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp had it)
)
