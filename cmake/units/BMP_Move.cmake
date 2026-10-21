# Unit v5627: BMP_Move, the bump car's move (src/game/rides/BumperRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_BMP_Move.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_BMP_Move src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_Move_KEEP
  __Z8BMP_MoveP7tagBUMP   # BMP_Move(tagBUMP*)
  _gBumpTurnMax
  _gBumpTurnMin
)
