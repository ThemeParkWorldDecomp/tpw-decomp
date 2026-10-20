# Unit v5L17: the online list destructors of the online system's teardown (src/FS_OnlineList_dtors.cpp). The park list's destructor and the
# park instance of the base (and with it the rest of unit v5921's file), and unit v51065's file for the city list's destructor, are
# compiled too: the test runs the host's real destructor chains. The callees are fakes in tests/unit_FS_OnlineList_dtors.cpp.
set(TPW_UNIT_FS_OnlineList_dtors
    src/FS_OnlineList_dtors.cpp
    src/FS_Online_gametype_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.cpp
    src/CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.cpp
    src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList_CFSOnlineSystemCityList_ReturnCityInfo.cpp
    platform/arena.cpp)
