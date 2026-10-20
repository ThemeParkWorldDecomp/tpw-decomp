# Unit w128: the flags of the park (DC_AnimateFlag, DC_DrawFlag; src/DynamicFlags.cpp). The only callee outside the file,
# DC_AddPolyFaceToList (src/DynamicFaces.cpp, unit w127), is a recording fake in tests/unit_DynamicFlags.cpp, which also defines the two grids.
# The strings (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_DynamicFlags src/DynamicFlags.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
