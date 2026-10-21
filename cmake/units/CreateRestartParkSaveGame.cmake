# Unit v5549: CreateRestartParkSaveGame (src/game/GameSys.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateRestartParkSaveGame.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateRestartParkSaveGame src/game/GameSys.cpp platform/arena.cpp)
set(TPW_UNIT_CreateRestartParkSaveGame_KEEP
  __Z25CreateRestartParkSaveGamev   # CreateRestartParkSaveGame()
)
