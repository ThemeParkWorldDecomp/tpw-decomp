# Unit v5935: CFSOnlineSystem::ConnectUploadServer and ::LoggedInRequestFTPDownload (src/game/online/FS_Online.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystem_ConnectUploadServer.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_ConnectUploadServer src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystem_ConnectUploadServer_KEEP
  __ZN15CFSOnlineSystem19ConnectUploadServerEv   # CFSOnlineSystem::ConnectUploadServer()
  __ZN15CFSOnlineSystem26LoggedInRequestFTPDownloadEPK13CFSOnlinePark   # CFSOnlineSystem::LoggedInRequestFTPDownload(CFSOnlinePark const*)
)
