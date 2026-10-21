# Unit v5640: operator<<(wostream&, const wchar_t*) (src/game/MslSupport.cpp). The callees are recording fakes in tests/unit_std_operator.cpp.
set(TPW_UNIT_std_operator src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_operator_KEEP
  __ZN13ignore_callee12OstreamWriteEPNS_12IgnoreStreamEPKDs   # ignore_callee::OstreamWrite(ignore_callee::IgnoreStream*, char16_t const*)
)
