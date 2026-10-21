# Unit w209: the vehicles' path following (Follow_Path::calculate, Vehicle::next_step, recalc_path and is_stuck, Pilot::init_behaviours and the Pilot constructor in
# src/game/rides/vehicle.cpp; the two map callbacks of next_step in src/game/rides/VehicleCallbacks.cpp). The steering code of unit w203 (src/game/rides/vehicle.cpp: truncate,
# normalise, perp, is_finished_path, set_speed, clear_behaviours and the globals) is real, as are FixMul, FixDiv and isqrt (src/game/park/GoldenTickets.cpp, src/game/ui/vector2d.cpp);
# wallhug_tricky, the three CMap calls, implement_nav_rulesA2B, CTRACE and the two candidate walks are recording fakes in tests/unit_VehiclePath.cpp.
set(TPW_UNIT_VehiclePath src/game/rides/vehicle.cpp src/game/rides/VehicleCallbacks.cpp src/game/park/GoldenTickets.cpp
    src/game/ui/vector2d.cpp platform/arena.cpp)
set(TPW_UNIT_VehiclePath_KEEP
  __Z13do_separationRK7VehiclePS0_   # do_separation(Vehicle const&, Vehicle const*)
  __Z17do_avoid_obstacleRK7VehiclePK8Obstacle   # do_avoid_obstacle(Vehicle const&, Obstacle const*)
  __Z17do_repel_obstacleRK7VehiclePK8Obstacle   # do_repel_obstacle(Vehicle const&, Obstacle const*)
  __Z19map_global_to_localjj   # map_global_to_local(unsigned int, unsigned int)
  __Z21line_circle_intersectR8Vector2dS0_S0_i   # line_circle_intersect(Vector2d&, Vector2d&, Vector2d&, int)
  __Z22xaxis_circle_intersectR8Vector2di   # xaxis_circle_intersect(Vector2d&, int)
  __Z4perpRK8Vector2d   # perp(Vector2d const&)
  __Z5isqrtj   # isqrt(unsigned int)
  __Z6FixDivii   # FixDiv(int, int)
  __Z6FixMulii   # FixMul(int, int)
  __Z6map_fniiPv   # map_fn(int, int, void*)
  __Z8truncateRK8Vector2di   # truncate(Vector2d const&, int)
  __Z9normaliseRK8Vector2d   # normalise(Vector2d const&)
  __ZN10Separation9calculateER7Vehicle   # Separation::calculate(Vehicle&)
  __ZN11Follow_Path9calculateER7Vehicle   # Follow_Path::calculate(Vehicle&)
  __ZN14Avoid_Obstacle9calculateER7Vehicle   # Avoid_Obstacle::calculate(Vehicle&)
  __ZN14Repel_Obstacle9calculateER7Vehicle   # Repel_Obstacle::calculate(Vehicle&)
  __ZN4Seek9calculateER7Vehicle   # Seek::calculate(Vehicle&)
  __ZN5Pilot10new_targetEii   # Pilot::new_target(int, int)
  __ZN5Pilot15init_behavioursEv   # Pilot::init_behaviours()
  __ZN5Pilot4moveEv   # Pilot::move()
  __ZN5Pilot5resetEiiii   # Pilot::reset(int, int, int, int)
  __ZN5Pilot7get_posERiS0_   # Pilot::get_pos(int&, int&)
  __ZN5Pilot9set_speedEf   # Pilot::set_speed(float)
  __ZN5PilotC1Ev   # Pilot::Pilot()
  __ZN7Arrival9calculateER7Vehicle   # Arrival::calculate(Vehicle&)
  __ZN7Vehicle11recalc_pathERK8Vector2dNS_14RecalcPathModeE   # Vehicle::recalc_path(Vector2d const&, Vehicle::RecalcPathMode)
  __ZN7Vehicle16clear_behavioursEv   # Vehicle::clear_behaviours()
  __ZN7Vehicle8is_stuckEv   # Vehicle::is_stuck()
  __ZN7Vehicle9next_stepEv   # Vehicle::next_step()
  __ZN7VehicleD1Ev   # Vehicle::~Vehicle()
  __ZNK5Pilot16is_finished_pathEv   # Pilot::is_finished_path() const
  __ZNK7Vehicle11predict_posEi   # Vehicle::predict_pos(int) const
  __ZNK7Vehicle13vec_to_globalERK8Vector2d   # Vehicle::vec_to_global(Vector2d const&) const
  __ZNK7Vehicle14point_to_localERK8Vector2d   # Vehicle::point_to_local(Vector2d const&) const
  __ZNK7Vehicle16is_finished_pathEv   # Vehicle::is_finished_path() const
  _gVehAvoidNearest
  _gVehAvoidRange
  _gVehBehaviours
  _gVehMapData
  _gVehNavRules
  _gVehOne
  _gVehPath
  _gVehSteerSum
  _gVehZero
  _g_vector2d_xaxis
)
