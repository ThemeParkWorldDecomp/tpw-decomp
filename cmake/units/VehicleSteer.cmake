# Unit w203: the ride vehicles' steering code (the vector helpers perp, normalise and truncate, the Vehicle's geometry, clear_behaviours and the destructor, the
# five behaviours' calculate functions with their three callbacks, the Pilot's members; src/Vehicle_steer.cpp). FixMul, FixDiv, isqrt and
# xaxis_circle_intersect are the real sources (src/fixmath.cpp, src/vector2d.cpp); the two candidate walks, Vehicle::recalc_path and Vehicle::next_step
# are recording fakes in tests/unit_VehicleSteer.cpp, which also defines nothing else of other files.
set(TPW_UNIT_VehicleSteer src/Vehicle_steer.cpp src/fixmath.cpp src/vector2d.cpp platform/arena.cpp)
