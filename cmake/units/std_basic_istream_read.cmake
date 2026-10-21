# Unit v5014: std::basic_istream<char>::read and ::getline (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_istream_read.cpp, which also defines the globals of the exception object.
set(TPW_UNIT_std_basic_istream_read src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_istream_read_KEEP
  __ZN16BasicIstreamChar4readEPci   # BasicIstreamChar::read(char*, int)
  __ZN16BasicIstreamChar7getlineEPcic   # BasicIstreamChar::getline(char*, int, char)
)
