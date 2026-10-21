# Unit v5229: nocase wide string compare and substring constructor (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_basic_string_compare_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_compare_3 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_compare_3_KEEP
  __ZN13online_callee17NocaseWStringCopyEPvPKvjj   # online_callee::NocaseWStringCopy(void*, void const*, unsigned int, unsigned int)
  __ZN13online_callee20NocaseWStringCompareEPKvjjPKDsj   # online_callee::NocaseWStringCompare(void const*, unsigned int, unsigned int, char16_t const*, unsigned int)
)
