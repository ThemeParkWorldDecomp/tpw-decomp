# Unit v5659: the advisor's bump world (BMP_InitSystem, BMP_CloseSystem, BMP_FreeAll, BMP_OpenRide, BMP_CloseRide; src/game/rides/BumperRide.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_InitSystem.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_InitSystem src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_InitSystem_KEEP
  __Z11BMP_FreeAllv   # BMP_FreeAll()
  __Z12BMP_OpenRidei   # BMP_OpenRide(int)
  __Z13BMP_CloseRidei   # BMP_CloseRide(int)
  __Z14BMP_InitSystemv   # BMP_InitSystem()
  __Z15BMP_CloseSystemv   # BMP_CloseSystem()
)
