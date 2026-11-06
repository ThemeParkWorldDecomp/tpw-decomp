# Unit v5049: CFSOnlineServer::CFSOnlineServer (src/game/online/FS_OnlineList.cpp). The string members' constructor and
# assignments are recording fakes in tests/unit_CFSOnlineServer_CFSOnlineServer.cpp.
set(TPW_UNIT_CFSOnlineServer_CFSOnlineServer src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineServer_CFSOnlineServer_KEEP
  __Z28CFSOnlineServer_DestructBodyP15CFSOnlineServer   # CFSOnlineServer_DestructBody(CFSOnlineServer*)
  __ZN17onlinelist_callee10ServerFindEP9TbPtrListI15CFSOnlineServerEPS1_   # onlinelist_callee::ServerFind(TbPtrList<CFSOnlineServer>*, CFSOnlineServer*)
  __ZN17onlinelist_callee14ServerRemoveAtEP9TbPtrListI15CFSOnlineServerERPv   # onlinelist_callee::ServerRemoveAt(TbPtrList<CFSOnlineServer>*, void*&)
  __ZN15CFSOnlineServerC1EPK12ccServerInfoP25CFSOnlineSystemServerList   # CFSOnlineServer::CFSOnlineServer(ccServerInfo const*, CFSOnlineSystemServerList*)
  __ZN15CFSOnlineServerD1Ev   # CFSOnlineServer::~CFSOnlineServer()
)
