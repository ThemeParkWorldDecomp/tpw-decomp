# Unit w104: the way out of the lobby. CGame's theme setters and ConverttoCurrentTheme, KeyParticlePOUFFE, DeInitLobby and DeInitLobbyUI,
# IsRideFirstPersonRide, ESP_RemoveSprites, the online globe's destructor, Close and list helpers. Real: those, the rectangle
# (src/UI_Base.cpp) and the light (src/Light.cpp). The strings' copy constructor, destructor and assignment, the master save, the map,
# the windows, the emitter and sound calls, the sprite file and list, the screen base class and the globe's entries are recording
# fakes in the unit file, which includes tests/calllog.cpp itself (it keeps the blocks the program frees).
set(TPW_UNIT_LobbyExit src/Game_theme_set.cpp src/lobby_exit.cpp src/hlsprite_remove.cpp src/lobby_globe_close.cpp src/UI_Base.cpp
    src/Light.cpp platform/arena.cpp)
set(TPW_UNIT_LobbyExit_OWN_CALLLOG ON)
