# Unit v5094: SwearFilter::Filter and SwearFilter::LoadWordList (src/game/online/IPBase.cpp). The callees outside the unit are recording fakes in
# tests/unit_SwearFilter_Filter.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_SwearFilter_Filter src/game/online/IPBase.cpp platform/arena.cpp)
set(TPW_UNIT_SwearFilter_Filter_KEEP
  __ZN11SwearFilter12LoadWordListEPKcRNSt3__14listINS2_12basic_stringIDsNS2_11char_traitsIDsEENS2_9allocatorIDsEEEENS7_IS9_EEEEb   # SwearFilter::LoadWordList(char const*, std::__1::list<std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>, std::__1::allocator<std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>>>&, bool)
  __ZN11SwearFilter6FilterERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEEb   # SwearFilter::Filter(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&, bool)
)
