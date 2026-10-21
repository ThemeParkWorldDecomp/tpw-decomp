# Unit v5672: BMP_DrawCarsForRide (src/game/rides/BumperRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_BMP_DrawCarsForRide.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_BMP_DrawCarsForRide src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_DrawCarsForRide_KEEP
  __Z19BMP_DrawCarsForRideif   # BMP_DrawCarsForRide(int, float)
  _gDrawCarsAngles
  _gDrawCarsClampHi
  _gDrawCarsClampLo
  _gDrawCarsInit
  _gDrawCarsScale
)
