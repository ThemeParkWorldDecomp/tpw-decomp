# Unit v5665: std::__use_facet for ctype<char>, codecvt<char,char,int>, ctype<wchar_t>, codecvt<wchar_t,char,int>
# (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std___use_facet.cpp, which also defines the id globals.
set(TPW_UNIT_std___use_facet src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___use_facet_KEEP
  __Z17UseFacetCtypeCharPK14UseFacetLocale   # UseFacetCtypeChar(UseFacetLocale const*)
  __Z18UseFacetCtypeWcharPK14UseFacetLocale   # UseFacetCtypeWchar(UseFacetLocale const*)
  __Z23UseFacetCodecvtCharCharPK14UseFacetLocale   # UseFacetCodecvtCharChar(UseFacetLocale const*)
  __Z24UseFacetCodecvtWcharCharPK14UseFacetLocale   # UseFacetCodecvtWcharChar(UseFacetLocale const*)
)
