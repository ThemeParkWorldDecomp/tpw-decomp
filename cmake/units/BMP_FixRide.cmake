# Unit v5220: the ride commands of the bump world (BMP_HaltRide, BMP_BreakRide, BMP_FixRide, BMP_BeginRide; src/game/rides/BumperRide.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_FixRide.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_FixRide src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_FixRide_KEEP
  __Z11BMP_FixRidei   # BMP_FixRide(int)
  __Z12BMP_HaltRidei   # BMP_HaltRide(int)
  __Z13BMP_BeginRidei   # BMP_BeginRide(int)
  __Z13BMP_BreakRidei   # BMP_BreakRide(int)
)
