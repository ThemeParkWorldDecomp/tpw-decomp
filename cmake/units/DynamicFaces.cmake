# Unit w127: the dynamic face list and the water (sort, DC_ResetSquareFaceListPtr, DC_AddSquareFaceToList, DC_AddPolyFaceToList,
# HDZ_RotatePointZ, DC_AnimateWater, DC_DrawWater; src/DynamicFaces.cpp). The only callee outside the file, CTRACE, is a fake in
# tests/unit_DynamicFaces.cpp. The strings (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_DynamicFaces src/DynamicFaces.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
