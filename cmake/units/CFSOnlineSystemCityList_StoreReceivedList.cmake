# Unit v5307: the city list's ChildLocked, RequestListFromServer and StoreReceivedList (src/game/online/FS_OnlineList.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemCityList_StoreReceivedList.cpp.
set(TPW_UNIT_CFSOnlineSystemCityList_StoreReceivedList src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemCityList_StoreReceivedList_KEEP
  __ZN23CFSOnlineSystemCityList11ChildLockedEv   # CFSOnlineSystemCityList::ChildLocked()
  __ZN23CFSOnlineSystemCityList17StoreReceivedListEPK14ccReceivedListI10ccCityInfoE   # CFSOnlineSystemCityList::StoreReceivedList(ccReceivedList<ccCityInfo> const*)
  __ZN23CFSOnlineSystemCityList21RequestListFromServerEP4Cityj   # CFSOnlineSystemCityList::RequestListFromServer(City*, unsigned int)
)
