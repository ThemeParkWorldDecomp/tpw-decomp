# Unit v5046: std::basic_filebuf<char> construct, open, uflow (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_basic_filebuf_basic_filebuf.cpp.
set(TPW_UNIT_std_basic_filebuf_basic_filebuf src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_basic_filebuf_KEEP
  __ZN16BasicFilebufChar4openEPKci   # BasicFilebufChar::open(char const*, int)
  __ZN16BasicFilebufChar5uflowEv   # BasicFilebufChar::uflow()
  __ZN16BasicFilebufChar9constructEPv   # BasicFilebufChar::construct(void*)
  _gFilebufCharVtblA
  _gFilebufCharVtblB
)
