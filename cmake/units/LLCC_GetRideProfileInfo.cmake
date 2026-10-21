# Unit v5691: LLCC_GetRideProfileInfo and LLCC_StartPylonMove (src/game/park/LLCellRide.cpp).
# The callees outside the unit are recording fakes in tests/unit_LLCC_GetRideProfileInfo.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_GetRideProfileInfo src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_GetRideProfileInfo_KEEP
  __Z19LLCC_StartPylonMoveiii   # LLCC_StartPylonMove(int, int, int)
  __Z23LLCC_GetRideProfileInfoPv   # LLCC_GetRideProfileInfo(void*)
  _gLLCCDB7C
)
