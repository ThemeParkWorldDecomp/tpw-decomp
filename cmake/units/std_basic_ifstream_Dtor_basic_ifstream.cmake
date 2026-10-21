# Unit v5201: std::basic_ifstream<wchar_t>::~basic_ifstream (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_ifstream_Dtor_basic_ifstream.cpp, which also defines the globals.
set(TPW_UNIT_std_basic_ifstream_Dtor_basic_ifstream src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ifstream_Dtor_basic_ifstream_KEEP
  __ZN18BasicIfstreamWchar7destroyEs   # BasicIfstreamWchar::destroy(short)
)
