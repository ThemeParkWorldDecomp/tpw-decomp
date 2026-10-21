# Unit v5721: ServerListPostLoginCallbackFn (src/game/online/FS_OnlineList.cpp). The callees are recording fakes in
# tests/unit_ServerListPostLoginCallbackFn.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_ServerListPostLoginCallbackFn src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_ServerListPostLoginCallbackFn_KEEP
  __Z29ServerListPostLoginCallbackFniPKv   # ServerListPostLoginCallbackFn(int, void const*)
)
