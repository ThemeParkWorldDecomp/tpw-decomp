# Unit v5099: std::basic_string<wchar_t>::operator[] (src/game/MslSupport.cpp). The callees are recording fakes in
# tests/unit_std_basic_string_operator.cpp.
set(TPW_UNIT_std_basic_string_operator src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_operator_KEEP
  __ZN3IME19MslWideString_indexEPNS_13MslWideStringEj   # IME::MslWideString_index(IME::MslWideString*, unsigned int)
)
