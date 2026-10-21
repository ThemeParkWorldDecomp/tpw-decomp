# Unit v5708: std::basic_istream<wchar_t> ~basic_istream, getline and the constructor
# (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_basic_istream_Dtor_basic_istream_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_istream_Dtor_basic_istream_2 src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_istream_Dtor_basic_istream_2_KEEP
  __ZN13ignore_callee11IstreamCtorEPNS_12IgnoreStreamEsj   # ignore_callee::IstreamCtor(ignore_callee::IgnoreStream*, short, unsigned int)
  __ZN13ignore_callee11IstreamDtorEPNS_12IgnoreStreamEs   # ignore_callee::IstreamDtor(ignore_callee::IgnoreStream*, short)
  __ZN13ignore_callee14IstreamGetlineEPNS_12IgnoreStreamEPDsiDs   # ignore_callee::IstreamGetline(ignore_callee::IgnoreStream*, char16_t*, int, char16_t)
)
