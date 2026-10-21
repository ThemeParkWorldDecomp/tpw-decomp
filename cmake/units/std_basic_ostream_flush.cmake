# Unit v5663: std::basic_ostream<char>::flush (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_ostream_flush.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_ostream_flush src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ostream_flush_KEEP
  __Z12OstreamFlushP16BasicIstreamChar   # OstreamFlush(BasicIstreamChar*)
)
