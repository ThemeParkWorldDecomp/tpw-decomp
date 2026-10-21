# The theme and the new level (unit w117): CGame::InitTheme, InitLevel, InitLevel_Load, InitLevel_Initialise and LoadTheme, CWorld::SetupNewLevel,
# CFSLoadSave::Load and LoadInternal, the four cell cost setters and the empty CSkyObjects constructor. The hoardings tables, the main balance
# (its description rows) and the strings' own sources are linked for the objects the functions use; CBalanceTable's reader, the ride database,
# the thing constructors, the engine and every other callee are fakes in the unit file.
set(TPW_UNIT_NewLevel src/game/Game.cpp src/game/World.cpp src/game/save/FS_LoadSave.cpp src/game/park/LLCellSystem.cpp
    src/game/park/SkyObjects.cpp src/game/balance/HoardingsBalance.cpp src/game/balance/MainBalance.cpp)
set(TPW_UNIT_NewLevel_KEEP
  __Z22LLCC_SystemSetPathCosti   # LLCC_SystemSetPathCost(int)
  __Z23LLCC_SystemSetQueueCosti   # LLCC_SystemSetQueueCost(int)
  __Z27LLCC_SystemSetKartTrackCosti   # LLCC_SystemSetKartTrackCost(int)
  __Z28LLCC_SystemSetWaterTrackCosti   # LLCC_SystemSetWaterTrackCost(int)
  __ZN11CFSLoadSave12LoadInternalERK23TbDynamicStringTemplateIDsES3_   # CFSLoadSave::LoadInternal(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSLoadSave4LoadERK23TbDynamicStringTemplateIDsE   # CFSLoadSave::Load(TbDynamicStringTemplate<char16_t> const&)
  __ZN11CSkyObjectsC1Ev   # CSkyObjects::CSkyObjects()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN17CHoardingsBalance21GetBalanceDescriptionEi   # CHoardingsBalance::GetBalanceDescription(int)
  __ZN17CHoardingsBalance21GetBalanceValuesStartEv   # CHoardingsBalance::GetBalanceValuesStart()
  __ZN17CHoardingsBalanceD1Ev   # CHoardingsBalance::~CHoardingsBalance()
  __ZN5CGame14InitLevel_LoadEv   # CGame::InitLevel_Load()
  __ZN5CGame20InitLevel_InitialiseEv   # CGame::InitLevel_Initialise()
  __ZN5CGame9InitLevelEv   # CGame::InitLevel()
  __ZN5CGame9InitThemeEPc   # CGame::InitTheme(char*)
  __ZN5CGame9LoadThemeEPKc   # CGame::LoadTheme(char const*)
  __ZN6CWorld13SetupNewLevelEPc   # CWorld::SetupNewLevel(char*)
  _gGlobalAmbientColour
  _gHoardingsBalance4130
  _gHoardingsBalance434C
  _gHoardingsBalanceDescriptions
  _gLLCC47D0
  _gLLCC47D4
  _gLLCC47D8
  _gLLCC47DC
  _gLevelRangeMinX
  _gLevelRangeMinY
  _gMainBalance
  _gMainBalanceDescriptions
  _gWaterStartPos
  _gWorldGatesName
  _gWorldLightsName
  __ZN12_GLOBAL__N_114sHoardingsInitE   # sHoardingsInit (static object)
)
# The unit file includes calllog.cpp itself (it observes the allocations).
set(TPW_UNIT_NewLevel_OWN_CALLLOG ON)
