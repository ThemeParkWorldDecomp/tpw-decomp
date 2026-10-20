# Unit v5226: the lobby's chat satellite and the world lobby window's messages (ChatServerCallbackFn, ProcessGetChatSatellitePlayers,
# WorldLobbyControllerMsg; src/ProcessGetChatSatellitePlayers.cpp). The callees outside the unit are recording fakes in
# tests/unit_ProcessGetChatSatellitePlayers.cpp, which also defines the globals of other files.
set(TPW_UNIT_ProcessGetChatSatellitePlayers src/ProcessGetChatSatellitePlayers.cpp platform/arena.cpp)
