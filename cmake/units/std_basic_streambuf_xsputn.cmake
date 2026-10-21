# Unit v5185: basic_streambuf<wchar_t>::xsputn and xsgetn (src/game/MslSupport.cpp). wmemcpy and the virtual calls are fakes in
# tests/unit_std_basic_streambuf_xsputn.cpp.
set(TPW_UNIT_std_basic_streambuf_xsputn src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_streambuf_xsputn_KEEP
  __ZN13MslWStreambuf6xsgetnEPDsi   # MslWStreambuf::xsgetn(char16_t*, int)
  __ZN13MslWStreambuf6xsputnEPKDsi   # MslWStreambuf::xsputn(char16_t const*, int)
)
