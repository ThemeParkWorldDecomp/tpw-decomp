# Unit v5765: Sock::Close (src/game/online/IPBase.cpp). The socket imports are recording fakes in
# tests/unit_Sock_Close.cpp.
set(TPW_UNIT_Sock_Close src/game/online/IPBase.cpp platform/arena.cpp)
set(TPW_UNIT_Sock_Close_KEEP
  __ZN4Sock5CloseEv   # Sock::Close()
)
