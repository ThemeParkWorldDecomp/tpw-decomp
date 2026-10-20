# Unit v51110: the server and ride lists' destructors, search members and LoggedInGetList, the FS_OnlineList static initialiser and three
# one-liners (src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp). The callees outside
# the unit are recording fakes in tests/unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemServerList_ReturnFTPDownloadServe src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp platform/arena.cpp)
