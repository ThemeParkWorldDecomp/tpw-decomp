# Unit v5146: std::basic_string<char> compare, substring constructor and append (src/game/MslSupport.cpp). The callees
# outside the unit are recording fakes in tests/unit_std_basic_string_compare_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_compare_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_compare_2_KEEP
  __ZN15msl_char_string6appendEP13MslCharStringjc   # msl_char_string::append(MslCharString*, unsigned int, char)
  __ZN15msl_char_string7compareEPK13MslCharStringjjPKcj   # msl_char_string::compare(MslCharString const*, unsigned int, unsigned int, char const*, unsigned int)
  __ZN15msl_char_string9constructEP13MslCharStringPKS0_jj   # msl_char_string::construct(MslCharString*, MslCharString const*, unsigned int, unsigned int)
  _gMslCharNul
)
