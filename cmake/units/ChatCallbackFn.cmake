# Unit v5212: ChatCallbackFn (src/game/online/online.cpp). The callees are recording fakes in
# tests/unit_ChatCallbackFn.cpp, which also defines gOnlineControl.
set(TPW_UNIT_ChatCallbackFn src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_ChatCallbackFn_KEEP
  __Z14ChatCallbackFniPvS_   # ChatCallbackFn(int, void*, void*)
)
