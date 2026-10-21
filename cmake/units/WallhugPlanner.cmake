# Unit w196: the walk planner (src/game/park/LineOfSight.cpp) with w195's line of sight real on the host (src/game/park/LineOfSight.cpp). The callees outside
# the two files (MAP_wall_in_way, UDebug) are recording fakes in tests/unit_WallhugPlanner.cpp.
set(TPW_UNIT_WallhugPlanner src/game/park/LineOfSight.cpp platform/arena.cpp)
set(TPW_UNIT_WallhugPlanner_KEEP
  __Z13line_of_sight16wallhug_waypointS_   # line_of_sight(wallhug_waypoint, wallhug_waypoint)
  __Z14wallhug_trickyP12wallhug_path7NavMode   # wallhug_tricky(wallhug_path*, NavMode)
  __Z15bresenham_start16wallhug_waypointS_   # bresenham_start(wallhug_waypoint, wallhug_waypoint)
  __Z15wallhug_cleanupP12wallhug_pathj   # wallhug_cleanup(wallhug_path*, unsigned int)
  __Z15wallhug_trivialP12wallhug_path7NavMode   # wallhug_trivial(wallhug_path*, NavMode)
  __Z21line_of_sight_cleanupP12wallhug_pathj   # line_of_sight_cleanup(wallhug_path*, unsigned int)
  __Z24wallhug_continue_trivialP12wallhug_path16wallhug_waypointj   # wallhug_continue_trivial(wallhug_path*, wallhug_waypoint, unsigned int)
  _gBresenhamDX
  _gBresenhamDY
  _gBresenhamError
  _gBresenhamXDir
  _gBresenhamXMajor
  _gBresenhamYDir
  _gWallhugHitCount
  _gWallhugHitDir
  _gWallhugHitSide
  _gWallhugHitStep
  _gWallhugNavMode
  _gWallhugRecordHits
  _gWallhugStep
  _gWallhugSteps
)
