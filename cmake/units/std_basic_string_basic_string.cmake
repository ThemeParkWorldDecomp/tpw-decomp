# Unit v5253: std::basic_string<char> constructors, destructor, init, assign, resize, append (src/game/MslSupport.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_basic_string_basic_string.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_basic_string src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_basic_string_KEEP
  __ZN15msl_char_string10assignFillEP13MslCharStringjc   # msl_char_string::assignFill(MslCharString*, unsigned int, char)
  __ZN15msl_char_string11appendCharsEP13MslCharStringPKcj   # msl_char_string::appendChars(MslCharString*, char const*, unsigned int)
  __ZN15msl_char_string11assignCharsEP13MslCharStringPKcj   # msl_char_string::assignChars(MslCharString*, char const*, unsigned int)
  __ZN15msl_char_string14constructEmptyEP13MslCharString   # msl_char_string::constructEmpty(MslCharString*)
  __ZN15msl_char_string17constructFromCStrEP13MslCharStringPKcPKv   # msl_char_string::constructFromCStr(MslCharString*, char const*, void const*)
  __ZN15msl_char_string4initEP13MslCharStringPKcS3_   # msl_char_string::init(MslCharString*, char const*, char const*)
  __ZN15msl_char_string6resizeEP13MslCharStringjc   # msl_char_string::resize(MslCharString*, unsigned int, char)
  __ZN15msl_char_string7destroyEP13MslCharStrings   # msl_char_string::destroy(MslCharString*, short)
  __ZN17msl_string_callee6MemcpyEPvPKvj   # msl_string_callee::Memcpy(void*, void const*, unsigned int)
  __ZN17msl_string_callee6MemsetEPvij   # msl_string_callee::Memset(void*, int, unsigned int)
  _gMslCtorNul
  _gMslInitNul
)
