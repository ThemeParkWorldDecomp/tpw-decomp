# Unit v5924: the online system's login and news jobs (LoginConnectionCallback, DoLogin, LoggedInGetNews, NewsConnectionCallback,
# CompleteNews; src/FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystem_LoginConnectionCallback.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_LoginConnectionCallback src/FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp platform/arena.cpp)
