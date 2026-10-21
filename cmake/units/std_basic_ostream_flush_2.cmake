# Unit v5898: std::basic_ostream<wchar_t>::flush and constructor (src/game/MslSupport.cpp). The callees are recording fakes in
# tests/unit_std_basic_ostream_flush_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_ostream_flush_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ostream_flush_2_KEEP
  __Z13WostreamFlushP9WostreamW   # WostreamFlush(WostreamW*)
  __Z17WostreamConstructP9WostreamWsj   # WostreamConstruct(WostreamW*, short, unsigned int)
  _gWostreamVtbl
)
