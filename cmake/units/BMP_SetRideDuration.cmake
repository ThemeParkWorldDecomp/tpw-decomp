# Unit v51036: ride commands on the bump world (src/game/balance/HoardingsBalance.cpp, src/game/rides/BumperRide.cpp). TRACE and
# CBalanceTable::Init are recording fakes in tests/unit_BMP_SetRideDuration.cpp, which also defines the world and the two tables.
set(TPW_UNIT_BMP_SetRideDuration src/game/balance/HoardingsBalance.cpp src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_SetRideDuration_KEEP
  __Z12BMP_RideWorni   # BMP_RideWorn(int)
  __Z13BMP_StartBumpiii   # BMP_StartBump(int, int, int)
  __Z13BMP_StartBuoyii   # BMP_StartBuoy(int, int)
  __Z17BMP_AddBuoyToRideP11tagBUMPRIDEii   # BMP_AddBuoyToRide(tagBUMPRIDE*, int, int)
  __Z17BMP_GetCameraMeshi   # BMP_GetCameraMesh(int)
  __Z19BMP_SetRideDurationii   # BMP_SetRideDuration(int, int)
  __Z28__sinit_HoardingsBalance_cppv   # __sinit_HoardingsBalance_cpp()
  _gHoardingsDtorVector
  _gHoardingsNode4130
  _gHoardingsNode434C
)
