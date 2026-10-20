# Unit w215: a coaster car's ride along the track (src/TPConfig_ridetick.cpp). The callees outside the unit are recording fakes in
# tests/unit_CarRideTick.cpp, which also defines the globals of other files. The matrix helpers (src/rsse_matrix.cpp, src/rsse_rotate.cpp)
# are the project's real ones. List only the sources the library must compile. The strings (src/TbString.cpp, src/TbStringBuffer.cpp,
# platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_CarRideTick src/TPConfig_ridetick.cpp src/rsse_matrix.cpp src/rsse_rotate.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
