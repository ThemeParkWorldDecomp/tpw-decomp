# Unit v5941: std::basic_string<char> replace 0x10002d20 and init 0x10072e6c (src/game/MslSupport.cpp). The callees outside the
# unit are recording fakes in tests/unit_std_basic_string_replace_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_replace_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_replace_2_KEEP
  __ZN15msl_char_string4initEP13MslCharStringjcPKv   # msl_char_string::init(MslCharString*, unsigned int, char, void const*)
  __ZN15msl_char_string7replaceEP13MslCharStringjjjc   # msl_char_string::replace(MslCharString*, unsigned int, unsigned int, unsigned int, char)
  _gMslInitFillNul
  _gMslReplaceNulCopy
  _gMslReplaceNulPlace
)
