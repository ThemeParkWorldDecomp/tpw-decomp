# Unit w203: the ride vehicles' steering code (the vector helpers perp, normalise and truncate, the Vehicle's geometry, clear_behaviours and the destructor, the
# five behaviours' calculate functions with their three callbacks, the Pilot's members; src/game/rides/vehicle.cpp). FixMul, FixDiv, isqrt and
# xaxis_circle_intersect are the real sources (src/game/park/GoldenTickets.cpp, src/game/ui/vector2d.cpp); the two candidate walks, Vehicle::recalc_path and Vehicle::next_step
# are recording fakes in tests/unit_VehicleSteer.cpp, which also defines nothing else of other files.
set(TPW_UNIT_VehicleSteer src/game/rides/vehicle.cpp src/game/park/GoldenTickets.cpp src/game/ui/vector2d.cpp
    platform/arena.cpp)
set(TPW_UNIT_VehicleSteer_KEEP
  __Z13do_separationRK7VehiclePS0_   # do_separation(Vehicle const&, Vehicle const*)
  __Z17do_avoid_obstacleRK7VehiclePK8Obstacle   # do_avoid_obstacle(Vehicle const&, Obstacle const*)
  __Z17do_repel_obstacleRK7VehiclePK8Obstacle   # do_repel_obstacle(Vehicle const&, Obstacle const*)
  __Z21line_circle_intersectR8Vector2dS0_S0_i   # line_circle_intersect(Vector2d&, Vector2d&, Vector2d&, int)
  __Z22xaxis_circle_intersectR8Vector2di   # xaxis_circle_intersect(Vector2d&, int)
  __Z4perpRK8Vector2d   # perp(Vector2d const&)
  __Z5isqrtj   # isqrt(unsigned int)
  __Z6FixDivii   # FixDiv(int, int)
  __Z6FixMulii   # FixMul(int, int)
  __Z8truncateRK8Vector2di   # truncate(Vector2d const&, int)
  __Z9normaliseRK8Vector2d   # normalise(Vector2d const&)
  __ZN10Separation9calculateER7Vehicle   # Separation::calculate(Vehicle&)
  __ZN14Avoid_Obstacle9calculateER7Vehicle   # Avoid_Obstacle::calculate(Vehicle&)
  __ZN14Repel_Obstacle9calculateER7Vehicle   # Repel_Obstacle::calculate(Vehicle&)
  __ZN4Seek9calculateER7Vehicle   # Seek::calculate(Vehicle&)
  __ZN5Pilot10new_targetEii   # Pilot::new_target(int, int)
  __ZN5Pilot4moveEv   # Pilot::move()
  __ZN5Pilot5resetEiiii   # Pilot::reset(int, int, int, int)
  __ZN5Pilot7get_posERiS0_   # Pilot::get_pos(int&, int&)
  __ZN5Pilot9set_speedEf   # Pilot::set_speed(float)
  __ZN7Arrival9calculateER7Vehicle   # Arrival::calculate(Vehicle&)
  __ZN7Vehicle16clear_behavioursEv   # Vehicle::clear_behaviours()
  __ZN7VehicleD1Ev   # Vehicle::~Vehicle()
  __ZNK5Pilot16is_finished_pathEv   # Pilot::is_finished_path() const
  __ZNK7Vehicle11predict_posEi   # Vehicle::predict_pos(int) const
  __ZNK7Vehicle13vec_to_globalERK8Vector2d   # Vehicle::vec_to_global(Vector2d const&) const
  __ZNK7Vehicle14point_to_localERK8Vector2d   # Vehicle::point_to_local(Vector2d const&) const
  __ZNK7Vehicle16is_finished_pathEv   # Vehicle::is_finished_path() const
  _gVehAvoidNearest
  _gVehAvoidRange
  _gVehBehaviours
  _gVehOne
  _gVehSteerSum
  _gVehZero
  _g_vector2d_xaxis
)
