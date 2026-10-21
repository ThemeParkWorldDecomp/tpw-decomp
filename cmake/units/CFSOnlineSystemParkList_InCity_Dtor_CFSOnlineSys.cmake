# Unit v5532: the InCity park list's destructor and RequestListFromServer (src/FS_Online_..._CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp).
# The base destructor's file is compiled too: the derived destructor relies on it. The callees are recording fakes in the unit's cpp.
set(TPW_UNIT_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys src/game/online/FS_Online.cpp
    src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys_KEEP
  __ZN23CFSOnlineSystemParkListD1Ev   # CFSOnlineSystemParkList::~CFSOnlineSystemParkList()
  __ZN30CFSOnlineSystemParkList_InCity21RequestListFromServerEP4Cityj   # CFSOnlineSystemParkList_InCity::RequestListFromServer(City*, unsigned int)
  __ZN30CFSOnlineSystemParkList_InCityD1Ev   # CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity()
  TPW_V5532_STRONG@game/online/FS_Online.cpp   # the #define TPW_V5532_STRONG of src/game/online/FS_Online.cpp (a guard macro; src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp had it)
  TPW_V5532_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_V5532_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp had it)
  __ZN7TbTListIP13CFSOnlineParkS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
