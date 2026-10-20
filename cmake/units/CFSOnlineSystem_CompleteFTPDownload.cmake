# Unit v5655: the FTP download job of the online system (CompleteFTPDownload, FTPDownloadCallbackFn, RequestNextFileForFTPDownload,
# Instance; src/FS_Online_init_CFSOnlineSystem_CompleteFTPDownload.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystem_CompleteFTPDownload.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_CompleteFTPDownload src/FS_Online_init_CFSOnlineSystem_CompleteFTPDownload.cpp platform/arena.cpp)
