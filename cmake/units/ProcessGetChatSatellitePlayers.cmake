# Unit v5226: the lobby's chat satellite and the world lobby window's messages (ChatServerCallbackFn, ProcessGetChatSatellitePlayers,
# WorldLobbyControllerMsg; src/game/ui/ui_worldLobbyController.cpp). The callees outside the unit are recording fakes in
# tests/unit_ProcessGetChatSatellitePlayers.cpp, which also defines the globals of other files.
set(TPW_UNIT_ProcessGetChatSatellitePlayers src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_ProcessGetChatSatellitePlayers_KEEP
  __Z20ChatServerCallbackFniPvR14ArgumentVector   # ChatServerCallbackFn(int, void*, ArgumentVector&)
  __Z23WorldLobbyControllerMsgP15InterfaceWindowiii   # WorldLobbyControllerMsg(InterfaceWindow*, int, int, int)
  __Z30ProcessGetChatSatellitePlayersv   # ProcessGetChatSatellitePlayers()
  _gChatSatelliteAnswers
  _gChatSatelliteChat
  _gChatSatelliteFinished
  _gChatSatelliteParkIDs
  _gChatSatelliteValues
  _gChatSatelliteWanted
)
