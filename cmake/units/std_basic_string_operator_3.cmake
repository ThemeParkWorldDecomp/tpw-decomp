# Unit v51033: basic_string<char>::operator[], std::locale::locale, basic_ios<char>::widen, _RefCountedPtr dtor, basic_filebuf<char> close and
# underflow (src/game/MslSupport.cpp). The callees are recording fakes in tests/unit_std_basic_string_operator_3.cpp.
set(TPW_UNIT_std_basic_string_operator_3 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_operator_3_KEEP
  __Z13BasicIosWidenPK12BasicIosCharc   # BasicIosWiden(BasicIosChar const*, char)
  __Z15LocaleConstructP12LocaleRefPtr   # LocaleConstruct(LocaleRefPtr*)
  __Z16FilebufCharCloseP16BasicFilebufChar   # FilebufCharClose(BasicFilebufChar*)
  __Z18MslCharStringIndexP13MslCharStringj   # MslCharStringIndex(MslCharString*, unsigned int)
  __Z19LocaleRefPtrDestroyP12LocaleRefPtrs   # LocaleRefPtrDestroy(LocaleRefPtr*, short)
  __Z20FilebufCharUnderflowP16BasicFilebufChar   # FilebufCharUnderflow(BasicFilebufChar*)
)
