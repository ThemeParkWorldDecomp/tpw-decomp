# Unit v5025: std::basic_filebuf<wchar_t>::uflow (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_filebuf_uflow.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_filebuf_uflow src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_uflow_KEEP
  __ZN17BasicFilebufWchar5uflowEv   # BasicFilebufWchar::uflow()
)
