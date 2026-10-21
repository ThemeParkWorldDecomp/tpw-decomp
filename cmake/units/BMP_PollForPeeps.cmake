# Unit v5139: the bump ride's peep lists and BMP_LeaveBump (src/game/rides/BumperRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_BMP_PollForPeeps.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_PollForPeeps src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_PollForPeeps_KEEP
  __Z13BMP_LeaveBumpP7tagBUMP   # BMP_LeaveBump(tagBUMP*)
  __Z16BMP_PollForPeepsi   # BMP_PollForPeeps(int)
  __Z28BMP_AddPeepToRideJoiningListij   # BMP_AddPeepToRideJoiningList(int, unsigned int)
  __Z28BMP_AddPeepToRideLeavingListij   # BMP_AddPeepToRideLeavingList(int, unsigned int)
)
