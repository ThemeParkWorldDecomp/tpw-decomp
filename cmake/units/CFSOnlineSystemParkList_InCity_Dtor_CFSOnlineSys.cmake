# Unit v5532: the InCity park list's destructor and RequestListFromServer (src/FS_Online_..._CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp).
# The base destructor's file is compiled too: the derived destructor relies on it. The callees are recording fakes in the unit's cpp.
set(TPW_UNIT_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys
    src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp
    src/FS_Online_gametype_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.cpp
    platform/arena.cpp)
