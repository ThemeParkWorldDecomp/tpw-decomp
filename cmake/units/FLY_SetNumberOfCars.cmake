# Unit v5277: FLY base accessors (src/game/rides/FlyingRide.cpp). No callees; tests/unit_FLY_SetNumberOfCars.cpp defines gFlyBases.
set(TPW_UNIT_FLY_SetNumberOfCars src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_SetNumberOfCars_KEEP
  __Z13FLY_LaunchCari   # FLY_LaunchCar(int)
  __Z14FLY_SetRunTimeii   # FLY_SetRunTime(int, int)
  __Z15FLY_RideRunningi   # FLY_RideRunning(int)
  __Z15FLY_SetRunSpeedii   # FLY_SetRunSpeed(int, int)
  __Z17FLY_GetBrokenStati   # FLY_GetBrokenStat(int)
  __Z17FLY_SetOpenClosedii   # FLY_SetOpenClosed(int, int)
  __Z19FLY_SetNumberOfCarsii   # FLY_SetNumberOfCars(int, int)
)
