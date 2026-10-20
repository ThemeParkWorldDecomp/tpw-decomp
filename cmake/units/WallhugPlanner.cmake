# Unit w196: the walk planner (src/World_wallhug.cpp) with w195's line of sight real on the host (src/World_sight.cpp). The callees outside
# the two files (MAP_wall_in_way, UDebug) are recording fakes in tests/unit_WallhugPlanner.cpp.
set(TPW_UNIT_WallhugPlanner src/World_wallhug.cpp src/World_sight.cpp platform/arena.cpp)
