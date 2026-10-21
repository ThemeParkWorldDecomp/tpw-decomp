# Unit v5890: LoadRides (src/game/rides/TPRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LoadRides_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LoadRides_2 src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_LoadRides_2_KEEP
  __Z9LoadRidesPv   # LoadRides(void*)
  _gLoadRidesShape
  _gLoadRidesTbErrorZero
)
