# Unit v5198: GetNextWaypoint (src/game/rides/FlyingRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetNextWaypoint.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetNextWaypoint src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_GetNextWaypoint_KEEP
  __Z15GetNextWaypointP6FLYBOY   # GetNextWaypoint(FLYBOY*)
)
