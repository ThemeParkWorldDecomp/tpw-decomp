# Unit v51009: VoteNotifyCallback, SwearFilter::Init, stristr, IPBase::ShowError, FTPClient::ProcessEvents (src/game/online/FTPClient.cpp, src/game/online/IPBase.cpp, src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_VoteNotifyCallback.cpp, which also defines gOnlineControl.
set(TPW_UNIT_VoteNotifyCallback src/game/online/FTPClient.cpp src/game/online/IPBase.cpp src/game/online/online.cpp
    platform/arena.cpp)
set(TPW_UNIT_VoteNotifyCallback_KEEP
  __Z18VoteNotifyCallbackiiPvS_   # VoteNotifyCallback(int, int, void*, void*)
  __Z7stristrPDsS_   # stristr(char16_t*, char16_t*)
  __ZN10ftp_callee15ControlCallbackEP11CFTPControlii   # ftp_callee::ControlCallback(CFTPControl*, int, int)
  __ZN11SwearFilter4InitEPKcS1_b   # SwearFilter::Init(char const*, char const*, bool)
  __ZN12swear_callee9EraseListEPNSt3__14listINS0_12basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEENS5_IS7_EEEE   # swear_callee::EraseList(std::__1::list<std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>, std::__1::allocator<std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>>>*)
  __ZN6IPBase9ShowErrorEv   # IPBase::ShowError()
  __ZN9FTPClient13ProcessEventsEv   # FTPClient::ProcessEvents()
  __ZN9ip_callee15WSAGetLastErrorEv   # ip_callee::WSAGetLastError()
  _gIPErrorCodes
  _gIPErrorTexts
  _gSwearLowerMap
  __ZL14sLowerMapReady   # sLowerMapReady (static object)
)
