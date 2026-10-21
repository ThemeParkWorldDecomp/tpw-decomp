# Unit v5233: std::basic_filebuf<char> seekpos, seekoff, overflow (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_basic_filebuf_seekpos.cpp.
set(TPW_UNIT_std_basic_filebuf_seekpos src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_seekpos_KEEP
  __ZN16BasicFilebufChar7seekoffEiij   # BasicFilebufChar::seekoff(int, int, unsigned int)
  __ZN16BasicFilebufChar7seekposEiij   # BasicFilebufChar::seekpos(int, int, unsigned int)
  __ZN16BasicFilebufChar8overflowEi   # BasicFilebufChar::overflow(int)
)
