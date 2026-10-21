# The lobby's small leaves part 7 (unit w81): CGame::ClearThemeResources, CRideDatabase::ClearDatabase, StripNodeWrapping, activateUI,
# UIInterface::Pause, CWorld::ExitLevel/Free/FreeThingHeap, CStaffDB::~CStaffDB, CSkyObjects::DestroySkyObjects. Everything they call is a
# fake in the unit file, but TPGlobals.cpp (the real default names the theme clean-up looks the resources up by).
set(TPW_UNIT_LobbyLeaves7 src/game/rides/RideDB.cpp src/game/Game.cpp src/game/rides/RidePlacement.cpp
    src/game/ui/UI_Base.cpp src/game/ui/UIInterface.cpp src/game/World.cpp src/game/people/StaffDB.cpp
    src/game/park/SkyObjects.cpp src/game/TPGlobals.cpp platform/arena.cpp)
set(TPW_UNIT_LobbyLeaves7_KEEP
  __Z10activateUIP15InterfaceWindowi   # activateUI(InterfaceWindow*, int)
  __Z17StripNodeWrappingP15tag_sM3D2HeaderP9tag_sNodefPvP8s3DPointRi   # StripNodeWrapping(tag_sM3D2Header*, tag_sNode*, float, void*, s3DPoint*, int&)
  __ZN11CSkyObjects17DestroySkyObjectsEv   # CSkyObjects::DestroySkyObjects()
  __ZN11UIInterface5PauseEv   # UIInterface::Pause()
  __ZN12CRideBalance21GetBalanceDescriptionEi   # CRideBalance::GetBalanceDescription(int)
  __ZN12CRideBalance21GetBalanceValuesStartEv   # CRideBalance::GetBalanceValuesStart()
  __ZN13CRideDatabase13ClearDatabaseEv   # CRideDatabase::ClearDatabase()
  __ZN5CGame19ClearThemeResourcesEv   # CGame::ClearThemeResources()
  __ZN6CWorld13FreeThingHeapEv   # CWorld::FreeThingHeap()
  __ZN6CWorld4FreeEv   # CWorld::Free()
  __ZN6CWorld9ExitLevelEv   # CWorld::ExitLevel()
  __ZN8CStaffDBD1Ev   # CStaffDB::~CStaffDB()
  __ZN9TPGlobalsC1Ev   # TPGlobals::TPGlobals()
  __ZN9TPGlobalsD1Ev   # TPGlobals::~TPGlobals()
  _gRideBalanceDescriptions
)
