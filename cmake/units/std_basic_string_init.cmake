# Unit v5645: std::basic_string<wchar_t> init, constructors and change_size (src/game/MslSupport.cpp). The callees outside
# the unit are recording fakes in tests/unit_std_basic_string_init.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_init src/game/MslSupport.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_init_KEEP
  __ZN3IME16gMslWcharNulInitE   # IME::gMslWcharNulInit
  __ZN3IME17gMslWcharNulEmptyE   # IME::gMslWcharNulEmpty
  __ZN3IME18MslWideString_ctorEPNS_13MslWideStringEPKDsj   # IME::MslWideString_ctor(IME::MslWideString*, char16_t const*, unsigned int)
  __ZN3IME18MslWideString_initEPNS_13MslWideStringEPKDsS3_   # IME::MslWideString_init(IME::MslWideString*, char16_t const*, char16_t const*)
  __ZN3IME18gMslWcharNulResizeE   # IME::gMslWcharNulResize
  __ZN3IME22MslWideString_ctorCStrEPNS_13MslWideStringEPKDs   # IME::MslWideString_ctorCStr(IME::MslWideString*, char16_t const*)
  __ZN3IME23MslWideString_ctorEmptyEPNS_13MslWideStringE   # IME::MslWideString_ctorEmpty(IME::MslWideString*)
  __ZN3IME24MslWideString_changeSizeEPNS_13MslWideStringEjb   # IME::MslWideString_changeSize(IME::MslWideString*, unsigned int, bool)
)
