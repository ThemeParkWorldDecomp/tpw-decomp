# Unit v5082: LLCC_Kill_Them_KILL_THEM_ALL and LLCC_BuildHoardingDB (src/game/park/LLCellRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCC_Kill_Them_KILL_THEM_ALL.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_Kill_Them_KILL_THEM_ALL src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_Kill_Them_KILL_THEM_ALL_KEEP
  __Z20LLCC_BuildHoardingDBiiiicb   # LLCC_BuildHoardingDB(int, int, int, int, char, bool)
  __Z28LLCC_Kill_Them_KILL_THEM_ALLiiii   # LLCC_Kill_Them_KILL_THEM_ALL(int, int, int, int)
  _gLLCCHoardMaxX
  _gLLCCHoardMaxY
  _gLLCCHoardMinX
  _gLLCCHoardMinY
)
