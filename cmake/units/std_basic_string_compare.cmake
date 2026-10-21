# Unit v5008: std::basic_string<wchar_t> compare, assign, substring constructor (src/game/MslSupport.cpp). The callees
# outside the unit are recording fakes in tests/unit_std_basic_string_compare.cpp.
set(TPW_UNIT_std_basic_string_compare src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_compare_KEEP
  __ZN3IME12gMslExcVtblAE   # IME::gMslExcVtblA
  __ZN3IME12gMslExcVtblBE   # IME::gMslExcVtblB
  __ZN3IME12gMslExcVtblCE   # IME::gMslExcVtblC
  __ZN3IME20MslWideString_assignEPNS_13MslWideStringEPKS0_jj   # IME::MslWideString_assign(IME::MslWideString*, IME::MslWideString const*, unsigned int, unsigned int)
  __ZN3IME20gMslOutOfRangeDtorTVE   # IME::gMslOutOfRangeDtorTV
  __ZN3IME21MslWideString_compareEPKNS_13MslWideStringEjjPKDsj   # IME::MslWideString_compare(IME::MslWideString const*, unsigned int, unsigned int, char16_t const*, unsigned int)
  __ZN3IME23MslWideString_constructEPNS_13MslWideStringEPKS0_jj   # IME::MslWideString_construct(IME::MslWideString*, IME::MslWideString const*, unsigned int, unsigned int)
)
