# Unit v5101: FTPClient::RecvData, ~FTPClient, Connect, _EnterPassiveMode (src/game/online/FTPClient.cpp). The callees outside the unit are
# recording fakes in tests/unit_FTPClient_RecvData.cpp.
set(TPW_UNIT_FTPClient_RecvData src/game/online/FTPClient.cpp platform/arena.cpp)
set(TPW_UNIT_FTPClient_RecvData_KEEP
  __ZN9FTPClient17_EnterPassiveModeEv   # FTPClient::_EnterPassiveMode()
  __ZN9FTPClient7ConnectEPcS0_S0_S0_   # FTPClient::Connect(char*, char*, char*, char*)
  __ZN9FTPClient8DestructEs   # FTPClient::Destruct(short)
  __ZN9FTPClient8RecvDataEbi   # FTPClient::RecvData(bool, int)
  __ZN9FTPClientD1Ev   # FTPClient::~FTPClient()
)
