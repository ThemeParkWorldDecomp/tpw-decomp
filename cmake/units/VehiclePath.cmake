# Unit w209: the vehicles' path following (Follow_Path::calculate, Vehicle::next_step, recalc_path and is_stuck, Pilot::init_behaviours and the Pilot constructor in
# src/Vehicle_path.cpp; the two map callbacks of next_step in src/VehicleCallbacks_map.cpp). The steering code of unit w203 (src/Vehicle_steer.cpp: truncate,
# normalise, perp, is_finished_path, set_speed, clear_behaviours and the globals) is real, as are FixMul, FixDiv and isqrt (src/fixmath.cpp, src/vector2d.cpp);
# wallhug_tricky, the three CMap calls, implement_nav_rulesA2B, CTRACE and the two candidate walks are recording fakes in tests/unit_VehiclePath.cpp.
set(TPW_UNIT_VehiclePath src/Vehicle_path.cpp src/VehicleCallbacks_map.cpp src/Vehicle_steer.cpp src/fixmath.cpp src/vector2d.cpp platform/arena.cpp)
