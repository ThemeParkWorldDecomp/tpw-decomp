# Unit v5014: std::basic_istream<char>::read and ::getline (src/std_basic_istream_read.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_istream_read.cpp, which also defines the globals of the exception object.
set(TPW_UNIT_std_basic_istream_read src/std_basic_istream_read.cpp platform/arena.cpp)
