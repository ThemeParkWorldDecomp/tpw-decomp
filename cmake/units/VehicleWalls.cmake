# Unit w218: the vehicles' wall avoidance and callbacks (Avoid_Walls::calculate in src/game/rides/vehicle.cpp; the nav rules, the two candidate walks and map_local_to_global
# in src/game/rides/VehicleCallbacks.cpp). The steering code of unit w203 (src/game/rides/vehicle.cpp: predict_pos and the globals), unit w209's map callbacks (map_global_to_local,
# map_fn in src/game/rides/VehicleCallbacks.cpp) and path code (src/game/rides/vehicle.cpp, which the globals of the nav rules sit in), FixMul, FixDiv, isqrt and
# line_circle_intersect (src/game/park/GoldenTickets.cpp, src/game/ui/vector2d.cpp) are real; CMap::IsXYValid, MAP_wall_in_way, GAME_ASSERTMSG and what Vehicle_path.cpp calls are recording fakes in tests/unit_VehicleWalls.cpp.
set(TPW_UNIT_VehicleWalls src/game/rides/vehicle.cpp src/game/rides/VehicleCallbacks.cpp
    src/game/park/GoldenTickets.cpp src/game/ui/vector2d.cpp platform/arena.cpp)
set(TPW_UNIT_VehicleWalls_KEEP
  __Z13do_separationRK7VehiclePS0_   # do_separation(Vehicle const&, Vehicle const*)
  __Z17do_avoid_obstacleRK7VehiclePK8Obstacle   # do_avoid_obstacle(Vehicle const&, Obstacle const*)
  __Z17do_repel_obstacleRK7VehiclePK8Obstacle   # do_repel_obstacle(Vehicle const&, Obstacle const*)
  __Z19map_global_to_localjj   # map_global_to_local(unsigned int, unsigned int)
  __Z19map_local_to_globaljj   # map_local_to_global(unsigned int, unsigned int)
  __Z21line_circle_intersectR8Vector2dS0_S0_i   # line_circle_intersect(Vector2d&, Vector2d&, Vector2d&, int)
  __Z22implement_nav_rulesA2Bii7NavMode   # implement_nav_rulesA2B(int, int, NavMode)
  __Z22xaxis_circle_intersectR8Vector2di   # xaxis_circle_intersect(Vector2d&, int)
  __Z28callback_obstacle_candidatesRK7VehicleiPFvS1_PK8ObstacleE   # callback_obstacle_candidates(Vehicle const&, int, void (*)(Vehicle const&, Obstacle const*))
  __Z29callback_collision_candidatesRK7VehicleiPFvS1_PS0_E   # callback_collision_candidates(Vehicle const&, int, void (*)(Vehicle const&, Vehicle const*))
  __Z4perpRK8Vector2d   # perp(Vector2d const&)
  __Z5isqrtj   # isqrt(unsigned int)
  __Z6FixDivii   # FixDiv(int, int)
  __Z6FixMulii   # FixMul(int, int)
  __Z6map_fniiPv   # map_fn(int, int, void*)
  __Z8truncateRK8Vector2di   # truncate(Vector2d const&, int)
  __Z9normaliseRK8Vector2d   # normalise(Vector2d const&)
  __ZN10Separation9calculateER7Vehicle   # Separation::calculate(Vehicle&)
  __ZN11Avoid_Walls9calculateER7Vehicle   # Avoid_Walls::calculate(Vehicle&)
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
  _gVehNavRulesAsserted
  _gVehNavRulesInit
  _gVehOne
  _gVehPath
  _gVehSteerSum
  _gVehZero
  _g_vector2d_xaxis
)
