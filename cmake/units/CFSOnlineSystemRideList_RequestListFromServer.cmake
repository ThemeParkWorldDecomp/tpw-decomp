# Unit v5718: CFSOnlineSystemRideList::RequestListFromServer and StoreReceivedList (src/game/online/FS_OnlineList.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemRideList_RequestListFromServer.cpp.
set(TPW_UNIT_CFSOnlineSystemRideList_RequestListFromServer src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemRideList_RequestListFromServer_KEEP
  __ZN23CFSOnlineSystemRideList17StoreReceivedListEPK14ccReceivedListI10ccRideInfoE   # CFSOnlineSystemRideList::StoreReceivedList(ccReceivedList<ccRideInfo> const*)
  __ZN23CFSOnlineSystemRideList21RequestListFromServerEP4Cityj   # CFSOnlineSystemRideList::RequestListFromServer(City*, unsigned int)
  TPW_V5718_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_V5718_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_OnlineList.cpp had it)
)
