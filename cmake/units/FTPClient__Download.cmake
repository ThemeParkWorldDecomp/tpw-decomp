# Unit v5227: FTPClient::_Connect and FTPClient::_Download (src/game/online/FTPClient.cpp). The callees outside the unit are
# recording fakes in tests/unit_FTPClient__Download.cpp.
set(TPW_UNIT_FTPClient__Download src/game/online/FTPClient.cpp platform/arena.cpp)
set(TPW_UNIT_FTPClient__Download_KEEP
  __ZN9FTPClient8_ConnectEv   # FTPClient::_Connect()
  __ZN9FTPClient9_DownloadEv   # FTPClient::_Download()
)
