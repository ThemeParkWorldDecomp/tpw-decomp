# Unit v5932: LLCR_OverRide (pointer overload), LLCM_TrackCellBoundary, LLCR_SetRide, LLCR_AddRideToDB (src/game/park/LLCellMouse.cpp, src/game/park/LLCellRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCR_OverRide_3.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_LLCR_OverRide_3 src/game/park/LLCellMouse.cpp src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCR_OverRide_3_KEEP
  __Z12LLCR_SetRide12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __Z13LLCR_OverRidePtPiS0_   # LLCR_OverRide(unsigned short*, int*, int*)
  __Z16LLCR_AddRideToDBi   # LLCR_AddRideToDB(int)
  __Z22LLCM_TrackCellBoundaryiiPiS_S_S_   # LLCM_TrackCellBoundary(int, int, int*, int*, int*, int*)
  _gLLCCE04C
  _gLLCCE092
  _gLLCCE094
  _gLLCCE098
  _gLLCCE09C
  _gLLCRRideShape
)
