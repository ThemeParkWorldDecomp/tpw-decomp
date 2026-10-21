# Unit v5113: the online login callbacks (src/game/online/FS_Online.cpp). The callees are recording fakes in
# tests/unit_FSOnlineLoginRASFailureCallback.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_FSOnlineLoginRASFailureCallback src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_FSOnlineLoginRASFailureCallback_KEEP
  __Z31FSOnlineLoginConnectionCallbackiiPvS_   # FSOnlineLoginConnectionCallback(int, int, void*, void*)
  __Z31FSOnlineLoginRASFailureCallbackv   # FSOnlineLoginRASFailureCallback()
)
