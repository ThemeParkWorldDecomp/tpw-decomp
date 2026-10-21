# Unit v5713: CFSOnlineSystemServerList::GetList, RequestListFromServer, StoreReceivedList (src/game/online/FS_OnlineList.cpp).
# The callees are recording fakes in tests/unit_CFSOnlineSystemServerList_RequestListFromServer.cpp.
set(TPW_UNIT_CFSOnlineSystemServerList_RequestListFromServer src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemServerList_RequestListFromServer_KEEP
  __ZN25CFSOnlineSystemServerList17StoreReceivedListEPK14ccReceivedListI12ccServerInfoE   # CFSOnlineSystemServerList::StoreReceivedList(ccReceivedList<ccServerInfo> const*)
  __ZN25CFSOnlineSystemServerList21RequestListFromServerEP4Cityj   # CFSOnlineSystemServerList::RequestListFromServer(City*, unsigned int)
  __ZN25CFSOnlineSystemServerList7GetListEv   # CFSOnlineSystemServerList::GetList()
  TPW_SERVERLIST_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_SERVERLIST_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_OnlineList.cpp had it)
)
