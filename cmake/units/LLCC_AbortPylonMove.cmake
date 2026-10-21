# Unit v5186: LLCC_AbortPylonMove, LLCC_IsRideMovable, LLCC_GetRideBlendInfo (src/game/park/LLCellRide.cpp). The callees outside
# the unit are recording fakes in tests/unit_LLCC_AbortPylonMove.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_AbortPylonMove src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_AbortPylonMove_KEEP
  __Z18LLCC_IsRideMovableP7CObject   # LLCC_IsRideMovable(CObject*)
  __Z19LLCC_AbortPylonMovev   # LLCC_AbortPylonMove()
  __Z21LLCC_GetRideBlendInfoPv   # LLCC_GetRideBlendInfo(void*)
  _gLLCCDF84
  _gLLCCE08C
  _gLLCCE0B4
  _gLLCCE0B8
)
