# The lobby's small leaves part 7 (unit w81): CGame::ClearThemeResources, CRideDatabase::ClearDatabase, StripNodeWrapping, activateUI,
# UIInterface::Pause, CWorld::ExitLevel/Free/FreeThingHeap, CStaffDB::~CStaffDB, CSkyObjects::DestroySkyObjects. Everything they call is a
# fake in the unit file, but TPGlobals.cpp (the real default names the theme clean-up looks the resources up by).
set(TPW_UNIT_LobbyLeaves7 src/RideDB_balance.cpp src/Game_theme_clear.cpp src/RideDB_clear.cpp src/NodeWrapping.cpp src/UI_activate.cpp src/UI_pause.cpp
    src/World_exit.cpp src/StaffDB_dtor.cpp src/SkyObjects_destroy.cpp src/TPGlobals.cpp src/TPGlobals_Dtor_TPGlobals.cpp platform/arena.cpp)
