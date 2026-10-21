# Unit v5093: std::basic_filebuf<wchar_t> seekoff, seekpos, overflow (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_basic_filebuf_seekoff.cpp.
set(TPW_UNIT_std_basic_filebuf_seekoff src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_seekoff_KEEP
  __ZN17BasicFilebufWchar7seekoffEiij   # BasicFilebufWchar::seekoff(int, int, unsigned int)
  __ZN17BasicFilebufWchar7seekposEiij   # BasicFilebufWchar::seekpos(int, int, unsigned int)
  __ZN17BasicFilebufWchar8overflowEDs   # BasicFilebufWchar::overflow(char16_t)
)
