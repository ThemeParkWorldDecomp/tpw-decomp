# Unit v5182: LLCM_SetCurrentObject and LLCM_StartTrackEditMode (src/game/park/LLCellMouse.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCM_SetCurrentObject.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCM_SetCurrentObject src/game/park/LLCellMouse.cpp platform/arena.cpp)
set(TPW_UNIT_LLCM_SetCurrentObject_KEEP
  __Z21LLCM_SetCurrentObjectb   # LLCM_SetCurrentObject(bool)
  __Z23LLCM_StartTrackEditModeiit   # LLCM_StartTrackEditMode(int, int, unsigned short)
  _gLLCC9F18
  _gLLCC9F1C
  _gLLCC9F20
  _gLLCCDB48
  _gLLCCDB5C
  _gLLCCE028
  _gLLCCE02A
  _gLLCCE034
  _gLLCCE038
  _gLLCMCurrentObject
)
