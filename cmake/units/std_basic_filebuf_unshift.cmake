# Unit v5743: std::basic_filebuf<wchar_t> unshift, sync, destructor (src/game/MslSupport.cpp). The callees are recording fakes in
# tests/unit_std_basic_filebuf_unshift.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_filebuf_unshift src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_unshift_KEEP
  __ZN17BasicFilebufWchar4syncEv   # BasicFilebufWchar::sync()
  __ZN17BasicFilebufWchar7destroyEs   # BasicFilebufWchar::destroy(short)
  __ZN17BasicFilebufWchar7unshiftEv   # BasicFilebufWchar::unshift()
)
