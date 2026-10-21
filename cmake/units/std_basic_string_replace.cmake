# Unit v5044: std::basic_string<wchar_t> replace, substr, begin (src/game/MslSupport.cpp). The callees outside the unit
# are recording fakes in tests/unit_std_basic_string_replace.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_replace src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_replace_KEEP
  __ZN3IME19MslWideString_beginEPNS_13MslWideStringE   # IME::MslWideString_begin(IME::MslWideString*)
  __ZN3IME20MslWideString_substrEPNS_13MslWideStringEPKS0_jj   # IME::MslWideString_substr(IME::MslWideString*, IME::MslWideString const*, unsigned int, unsigned int)
  __ZN3IME21MslWideString_replaceEPNS_13MslWideStringEjjjDs   # IME::MslWideString_replace(IME::MslWideString*, unsigned int, unsigned int, unsigned int, char16_t)
)
