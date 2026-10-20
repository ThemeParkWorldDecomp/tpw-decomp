# The theme and the new level (unit w117): CGame::InitTheme, InitLevel, InitLevel_Load, InitLevel_Initialise and LoadTheme, CWorld::SetupNewLevel,
# CFSLoadSave::Load and LoadInternal, the four cell cost setters and the empty CSkyObjects constructor. The hoardings tables, the main balance
# (its description rows) and the strings' own sources are linked for the objects the functions use; CBalanceTable's reader, the ride database,
# the thing constructors, the engine and every other callee are fakes in the unit file.
set(TPW_UNIT_NewLevel src/Game_level.cpp src/World_setup.cpp src/FS_LoadSave_load.cpp src/LLCellSystem_costs.cpp src/SkyObjects_ctor.cpp
    src/HoardingsBalance_objects.cpp src/HoardingsBalance.cpp src/MainBalance.cpp)
# The unit file includes calllog.cpp itself (it observes the allocations).
set(TPW_UNIT_NewLevel_OWN_CALLLOG ON)
