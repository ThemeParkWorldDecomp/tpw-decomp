# Unit v5216: HeightAtPos_NAV and GetNextHoldingWaypoint (src/game/rides/FlyingRide.cpp). GetWorldGSquareHeight is a recording fake in
# tests/unit_HeightAtPos_NAV.cpp, which also defines the globals of other files.
set(TPW_UNIT_HeightAtPos_NAV src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_HeightAtPos_NAV_KEEP
  __Z15HeightAtPos_NAVii   # HeightAtPos_NAV(int, int)
  __Z22GetNextHoldingWaypointP6FLYBOY   # GetNextHoldingWaypoint(FLYBOY*)
  _gFlyHoldMax
)
