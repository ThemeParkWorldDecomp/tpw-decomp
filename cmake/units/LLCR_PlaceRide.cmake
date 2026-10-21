# Unit v5635: LLCR_PlaceRide (src/game/park/LLCellRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCR_PlaceRide.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_LLCR_PlaceRide src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCR_PlaceRide_KEEP
  __Z14LLCR_PlaceRideiiiiii   # LLCR_PlaceRide(int, int, int, int, int, int)
  __Z19LLCR_PlaceRideBlockiiiiii   # LLCR_PlaceRideBlock(int, int, int, int, int, int)
  _gLLCRPlaceRotation
  _gLLCRPlaceX
  _gLLCRPlaceY
  _gLLCRPlacement
)
