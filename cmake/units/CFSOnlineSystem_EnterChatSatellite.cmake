# Unit v5130: the online system's chat satellite, publishing, upload and FTP download entry points and AreWeProcessing
# (src/game/online/FS_Online.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystem_EnterChatSatellite.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_EnterChatSatellite src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystem_EnterChatSatellite_KEEP
  __ZN15CFSOnlineSystem13SetPublishingEiPKcPKt   # CFSOnlineSystem::SetPublishing(int, char const*, unsigned short const*)
  __ZN15CFSOnlineSystem14CompleteUploadEi   # CFSOnlineSystem::CompleteUpload(int)
  __ZN15CFSOnlineSystem15AreWeProcessingEv   # CFSOnlineSystem::AreWeProcessing()
  __ZN15CFSOnlineSystem18EnterChatSatelliteEi   # CFSOnlineSystem::EnterChatSatellite(int)
  __ZN15CFSOnlineSystem18RequestFTPDownloadERK13CFSOnlineParkPFviPKvES4_   # CFSOnlineSystem::RequestFTPDownload(CFSOnlinePark const&, void (*)(int, void const*), void const*)
  __ZN15CFSOnlineSystem30RequestUploadPostLoginCallbackEiPKv   # CFSOnlineSystem::RequestUploadPostLoginCallback(int, void const*)
  __ZN15CFSOnlineSystem32GotoPlayerRequestFTPDownloadByIDEiPFviPKvES1_   # CFSOnlineSystem::GotoPlayerRequestFTPDownloadByID(int, void (*)(int, void const*), void const*)
  __ZN15CFSOnlineSystem36GotoPlayerFTPPostGetParkInfoCallbackEiPKvPK13CFSOnlinePark   # CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback(int, void const*, CFSOnlinePark const*)
)
