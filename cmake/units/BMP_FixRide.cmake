# Unit v5220: the ride commands of the bump world (BMP_HaltRide, BMP_BreakRide, BMP_FixRide, BMP_BeginRide; src/BMP_FixRide.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_FixRide.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_FixRide src/BMP_FixRide.cpp platform/arena.cpp)
