# Unit v5558: the ride creation (BMP_CreateRide; src/game/rides/BumperRide.cpp). The callees outside the unit are
# recording fakes in tests/unit_BMP_CreateRide.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_CreateRide src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_CreateRide_KEEP
  __Z14BMP_CreateRideiiiiii   # BMP_CreateRide(int, int, int, int, int, int)
)
