# Unit v5116: std::basic_ostream<char> ctor and dtor (src/game/MslSupport.cpp). The callees outside the unit
# are recording fakes in tests/unit_std_basic_ostream_Dtor_basic_ostream.cpp.
set(TPW_UNIT_std_basic_ostream_Dtor_basic_ostream src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ostream_Dtor_basic_ostream_KEEP
  __Z16BasicOstreamCtorP10MacOstreamsj   # BasicOstreamCtor(MacOstream*, short, unsigned int)
  __Z16BasicOstreamDtorP10MacOstreams   # BasicOstreamDtor(MacOstream*, short)
  _gIosBaseVt
  _gOstreamVt1
  _gOstreamVt2
)
