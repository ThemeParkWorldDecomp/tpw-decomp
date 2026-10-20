# Unit w218: the vehicles' wall avoidance and callbacks (Avoid_Walls::calculate in src/Vehicle_walls.cpp; the nav rules, the two candidate walks and map_local_to_global
# in src/VehicleCallbacks_nav.cpp). The steering code of unit w203 (src/Vehicle_steer.cpp: predict_pos and the globals), unit w209's map callbacks (map_global_to_local,
# map_fn in src/VehicleCallbacks_map.cpp) and path code (src/Vehicle_path.cpp, which the globals of the nav rules sit in), FixMul, FixDiv, isqrt and
# line_circle_intersect (src/fixmath.cpp, src/vector2d.cpp) are real; CMap::IsXYValid, MAP_wall_in_way, GAME_ASSERTMSG and what Vehicle_path.cpp calls are recording fakes in tests/unit_VehicleWalls.cpp.
set(TPW_UNIT_VehicleWalls src/Vehicle_walls.cpp src/VehicleCallbacks_nav.cpp src/Vehicle_path.cpp src/VehicleCallbacks_map.cpp src/Vehicle_steer.cpp src/fixmath.cpp src/vector2d.cpp platform/arena.cpp)
