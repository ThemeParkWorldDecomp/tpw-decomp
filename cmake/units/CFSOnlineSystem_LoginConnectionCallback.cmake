# Unit v5924: the online system's login and news jobs (LoginConnectionCallback, DoLogin, LoggedInGetNews, NewsConnectionCallback,
# CompleteNews; src/game/online/FS_Online.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystem_LoginConnectionCallback.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_LoginConnectionCallback src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystem_LoginConnectionCallback_KEEP
  __Z30FSOnlineNewsConnectionCallbackiiPvS_   # FSOnlineNewsConnectionCallback(int, int, void*, void*)
  __ZN13online_callee4FreeEPv   # online_callee::Free(void*)
  __ZN15CFSOnlineSystem12CompleteNewsEv   # CFSOnlineSystem::CompleteNews()
  __ZN15CFSOnlineSystem15LoggedInGetNewsEv   # CFSOnlineSystem::LoggedInGetNews()
  __ZN15CFSOnlineSystem22NewsConnectionCallbackEiiPvS0_   # CFSOnlineSystem::NewsConnectionCallback(int, int, void*, void*)
  __ZN15CFSOnlineSystem23LoginConnectionCallbackEiiPvS0_   # CFSOnlineSystem::LoginConnectionCallback(int, int, void*, void*)
  __ZN15CFSOnlineSystem7DoLoginEPKDsS1_   # CFSOnlineSystem::DoLogin(char16_t const*, char16_t const*)
  _gOnlineNewsGuid
)
