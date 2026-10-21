# Unit v5147: std::basic_streambuf<char> xsgetn / xsputn (src/game/MslSupport.cpp). The virtual callees are fakes in tests/unit_std_basic_streambuf_xsgetn.cpp.
set(TPW_UNIT_std_basic_streambuf_xsgetn src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_streambuf_xsgetn_KEEP
  __ZN20BasicStreambufCharIO6xsgetnEPci   # BasicStreambufCharIO::xsgetn(char*, int)
  __ZN20BasicStreambufCharIO6xsputnEPKci   # BasicStreambufCharIO::xsputn(char const*, int)
)
