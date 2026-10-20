# Unit w212: the coaster's track statistics (TPConfig.cpp; src/TPConfig_stats.cpp). The callees outside the unit (the trains' loading and
# activation, the sounds, the mesh draw switch, the rotation of a vector) are recording fakes in tests/unit_CoasterStats.cpp, which also
# defines gTPGlobals (the engine's sine table) and the ride animation sets' table.
# The strings (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_CoasterStats src/TPConfig_stats.cpp platform/arena.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
