# Unit v5656: SetNavRoute and BumpToPoint (src/game/rides/BumperRide.cpp). The callees are recording fakes in tests/unit_SetNavRoute.cpp.
set(TPW_UNIT_SetNavRoute src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_SetNavRoute_KEEP
  __Z11BumpToPointP7tagBUMPii   # BumpToPoint(tagBUMP*, int, int)
  __Z11SetNavRouteP7tagBUMPii   # SetNavRoute(tagBUMP*, int, int)
  _gBumpThrustLimitA
  _gBumpThrustLimitB
)
