# Unit v5371: Connection::RecvResponse and GetLocalIPAddress (src/game/online/Connection.cpp). The callees are
# recording fakes in tests/unit_Connection_RecvResponse.cpp.
set(TPW_UNIT_Connection_RecvResponse src/game/online/Connection.cpp platform/arena.cpp)
set(TPW_UNIT_Connection_RecvResponse_KEEP
  __ZN10Connection12RecvResponseEPcib   # Connection::RecvResponse(char*, int, bool)
  __ZN10Connection17GetLocalIPAddressEv   # Connection::GetLocalIPAddress()
)
