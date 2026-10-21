# Unit v5755: std::basic_ofstream<wchar_t> constructor (src/game/MslSupport.cpp). The callees are recording fakes in
# tests/unit_std_basic_ofstream_basic_ofstream.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_ofstream_basic_ofstream src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ofstream_basic_ofstream_KEEP
  __ZN13ignore_callee12OfstreamCtorEPNS_12IgnoreStreamEi   # ignore_callee::OfstreamCtor(ignore_callee::IgnoreStream*, int)
)
