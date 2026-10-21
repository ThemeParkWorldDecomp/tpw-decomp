# Unit v5999: LLCR_DeleteRide (src/game/park/LLCellRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCR_DeleteRide_2.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_LLCR_DeleteRide_2 src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCR_DeleteRide_2_KEEP
  __Z15LLCR_DeleteRideiii   # LLCR_DeleteRide(int, int, int)
  _gLLCRNoRide
)
