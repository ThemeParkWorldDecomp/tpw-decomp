# Unit v5700: std::operator<<(basic_ostream<char>&, const char*) (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_operator_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_operator_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_operator_2_KEEP
  __Z16OstreamWriteTextP14MacWriteStreamPKc   # OstreamWriteText(MacWriteStream*, char const*)
)
