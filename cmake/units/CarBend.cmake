# Unit w214: a coaster car bent to the track and a running train's sounds and screams (src/TPConfig_bend.cpp). The callees outside the unit are recording fakes in
# tests/unit_CarBend.cpp, which also defines the globals of other files. List only the sources the library must compile. The strings
# (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_CarBend src/TPConfig_bend.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
