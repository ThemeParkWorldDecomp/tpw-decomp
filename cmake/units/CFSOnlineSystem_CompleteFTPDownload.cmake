# Unit v5655: the FTP download job of the online system (CompleteFTPDownload, FTPDownloadCallbackFn, RequestNextFileForFTPDownload,
# Instance; src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystem_CompleteFTPDownload.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_CompleteFTPDownload src/game/online/FS_Online.cpp src/game/online/FS_OnlineList.cpp
    platform/arena.cpp tests/standins/CFSOnlineSystem_CompleteFTPDownload.cpp)
set(TPW_UNIT_CFSOnlineSystem_CompleteFTPDownload_KEEP
  __ZN12ftpdl_callee10RemoveHeadEP9TbPtrListI23TbDynamicStringTemplateIDsEE   # ftpdl_callee::RemoveHead(TbPtrList<TbDynamicStringTemplate<char16_t>>*)
  __ZN15CFSOnlineSystem19CompleteFTPDownloadEi   # CFSOnlineSystem::CompleteFTPDownload(int)
  __ZN15CFSOnlineSystem21FTPDownloadCallbackFnEii   # CFSOnlineSystem::FTPDownloadCallbackFn(int, int)
  __ZN15CFSOnlineSystem29RequestNextFileForFTPDownloadEv   # CFSOnlineSystem::RequestNextFileForFTPDownload()
  __ZN15CFSOnlineSystem8InstanceEv   # CFSOnlineSystem::Instance()
  _gFTPDownloadSlashA
  _gFTPDownloadSlashB
  _gFTPDownloadSlashC
)
