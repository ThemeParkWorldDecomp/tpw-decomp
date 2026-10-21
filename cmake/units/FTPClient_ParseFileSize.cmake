# Unit v5273: FTPClient constructor body, Disconnect, _Disconnect, Download, _CloseDSocket, ParseFileSize (src/game/online/FTPClient.cpp).
# The callees outside the unit are recording fakes in tests/unit_FTPClient_ParseFileSize.cpp.
set(TPW_UNIT_FTPClient_ParseFileSize src/game/online/FTPClient.cpp platform/arena.cpp)
set(TPW_UNIT_FTPClient_ParseFileSize_KEEP
  __ZN9FTPClient10DisconnectEv   # FTPClient::Disconnect()
  __ZN9FTPClient11_DisconnectEv   # FTPClient::_Disconnect()
  __ZN9FTPClient13ParseFileSizeEPc   # FTPClient::ParseFileSize(char*)
  __ZN9FTPClient13_CloseDSocketERi   # FTPClient::_CloseDSocket(int&)
  __ZN9FTPClient8DownloadEPcS0_   # FTPClient::Download(char*, char*)
  __ZN9FTPClient9ConstructEP11CFTPControlb   # FTPClient::Construct(CFTPControl*, bool)
)
