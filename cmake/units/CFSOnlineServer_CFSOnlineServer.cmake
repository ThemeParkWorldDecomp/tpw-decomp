# Unit v5049: CFSOnlineServer::CFSOnlineServer (src/game/online/FS_OnlineList.cpp). The string members' constructor and
# assignments are recording fakes in tests/unit_CFSOnlineServer_CFSOnlineServer.cpp.
set(TPW_UNIT_CFSOnlineServer_CFSOnlineServer src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineServer_CFSOnlineServer_KEEP
  __ZN15CFSOnlineServerC1EPK12ccServerInfoP25CFSOnlineSystemServerList   # CFSOnlineServer::CFSOnlineServer(ccServerInfo const*, CFSOnlineSystemServerList*)
  __ZN15CFSOnlineServerD1Ev   # CFSOnlineServer::~CFSOnlineServer()
)
