# Unit w170: rsse.cpp's matrix and vector helpers and FloatSortByArray (src/rsse_math.cpp). No callee outside the C library: atan2 and
# sqrt are the library's on the host side, fakes in the test module on the PowerPC side.
set(TPW_UNIT_RsseMath src/rsse_math.cpp platform/arena.cpp)
