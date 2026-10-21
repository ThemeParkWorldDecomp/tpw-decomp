# Unit v5104: std::basic_streambuf<char> uflow, imbue, getloc, destructor (src/game/MslSupport.cpp).
set(TPW_UNIT_std_basic_streambuf_uflow src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_streambuf_uflow_KEEP
  __ZN18BasicStreambufChar5imbueEPKv   # BasicStreambufChar::imbue(void const*)
  __ZN18BasicStreambufChar5uflowEv   # BasicStreambufChar::uflow()
  __ZN18BasicStreambufChar7destroyEs   # BasicStreambufChar::destroy(short)
  __ZNK18BasicStreambufChar6getlocEPv   # BasicStreambufChar::getloc(void*) const
)
