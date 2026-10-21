# Unit v5880: FLY_Tick (src/game/rides/FlyingRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_FLY_Tick.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_FLY_Tick src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_Tick_KEEP
  __Z8FLY_Tickv   # FLY_Tick()
  _gFlyApproachMinHeight
  _gFlyTakeOffMinHeight
  _gFlyTurnMinHeight
)
