# Unit v5630: the advisor save BMP_Save (src/game/rides/BumperRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_BMP_Save.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_BMP_Save src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_Save_KEEP
  __Z8BMP_SavePv   # BMP_Save(void*)
  _gBMPSaveDate
  _gBMPSaveTime
)
