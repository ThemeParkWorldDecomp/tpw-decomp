# Unit v5137: std::basic_filebuf<wchar_t> close, underflow, pbackfail, imbue (src/game/MslSupport.cpp). The callees outside
# the unit are recording fakes in tests/unit_std_basic_filebuf_close.cpp.
set(TPW_UNIT_std_basic_filebuf_close src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_close_KEEP
  __ZN17BasicFilebufWchar5closeEv   # BasicFilebufWchar::close()
  __ZN17BasicFilebufWchar5imbueEPKv   # BasicFilebufWchar::imbue(void const*)
  __ZN17BasicFilebufWchar9pbackfailEDs   # BasicFilebufWchar::pbackfail(char16_t)
  __ZN17BasicFilebufWchar9underflowEv   # BasicFilebufWchar::underflow()
)
