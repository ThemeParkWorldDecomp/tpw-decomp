# Unit v51028: SaveRides (src/game/rides/TPRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_SaveRides_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_SaveRides_2 src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_SaveRides_2_KEEP
  __Z9SaveRidesPv   # SaveRides(void*)
  _gSaveRidesTbErrorZero
)
