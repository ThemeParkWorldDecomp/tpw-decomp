# Unit w195: the line of sight and the path smoothing of the walk planner (src/game/park/LineOfSight.cpp). The callees outside the unit
# (MAP_wall_in_way, wallhug_trivial, UDebug) are recording fakes in tests/unit_LineOfSight.cpp.
set(TPW_UNIT_LineOfSight src/game/park/LineOfSight.cpp platform/arena.cpp)
set(TPW_UNIT_LineOfSight_KEEP
  __Z13line_of_sight16wallhug_waypointS_   # line_of_sight(wallhug_waypoint, wallhug_waypoint)
  __Z14wallhug_trickyP12wallhug_path7NavMode   # wallhug_tricky(wallhug_path*, NavMode)
  __Z15bresenham_start16wallhug_waypointS_   # bresenham_start(wallhug_waypoint, wallhug_waypoint)
  __Z15wallhug_cleanupP12wallhug_pathj   # wallhug_cleanup(wallhug_path*, unsigned int)
  __Z21line_of_sight_cleanupP12wallhug_pathj   # line_of_sight_cleanup(wallhug_path*, unsigned int)
  _gBresenhamDX
  _gBresenhamDY
  _gBresenhamError
  _gBresenhamXDir
  _gBresenhamXMajor
  _gBresenhamYDir
  _gWallhugNavMode
  _gWallhugStep
)
