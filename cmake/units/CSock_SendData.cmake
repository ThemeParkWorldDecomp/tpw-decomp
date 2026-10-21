# Unit v5304: CSock::CSock and CSock::SendData (src/game/online/Connection.cpp). Sock::Sock and send are recording fakes in
# tests/unit_CSock_SendData.cpp.
set(TPW_UNIT_CSock_SendData src/game/online/Connection.cpp platform/arena.cpp)
set(TPW_UNIT_CSock_SendData_KEEP
  __ZN5CSock8SendDataEPKci   # CSock::SendData(char const*, int)
  __ZN5CSockC1Ev   # CSock::CSock()
)
