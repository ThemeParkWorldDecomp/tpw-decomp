# Unit v5659: the advisor's bump world (BMP_InitSystem, BMP_CloseSystem, BMP_FreeAll, BMP_OpenRide, BMP_CloseRide; src/host_leaves_BMP_InitSystem.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_InitSystem.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_InitSystem src/host_leaves_BMP_InitSystem.cpp platform/arena.cpp)
