# Unit v5551: IsValidPylonPosition (src/game/rides/CoasterPylon.cpp). The callee outside the unit (XZAngleBetweenPylons) is a recording
# fake in tests/unit_IsValidPylonPosition.cpp, which also defines the globals of other files.
set(TPW_UNIT_IsValidPylonPosition src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_IsValidPylonPosition_KEEP
  __Z20IsValidPylonPositionjjPvjjjj   # IsValidPylonPosition(unsigned int, unsigned int, void*, unsigned int, unsigned int, unsigned int, unsigned int)
  _gPylonClampMax
  _gPylonClampMin
)
