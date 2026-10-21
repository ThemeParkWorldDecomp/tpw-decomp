# Unit v5151: GAMESYSTEM_Save and GAMESYSTEM_Load (src/game/GameSys.cpp). The callees outside the unit are recording fakes in
# tests/unit_GAMESYSTEM_Load.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_GAMESYSTEM_Load src/game/GameSys.cpp platform/arena.cpp)
set(TPW_UNIT_GAMESYSTEM_Load_KEEP
  __Z15GAMESYSTEM_LoadPv   # GAMESYSTEM_Load(void*)
  __Z15GAMESYSTEM_SavePv   # GAMESYSTEM_Save(void*)
)
