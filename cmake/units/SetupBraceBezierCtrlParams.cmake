# Unit v5992: SetupBraceBezierCtrlParams and GenerateTrack (src/game/rides/CoasterTrack.cpp). The callees outside the
# unit are recording fakes in tests/unit_SetupBraceBezierCtrlParams.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetupBraceBezierCtrlParams src/game/rides/CoasterTrack.cpp platform/arena.cpp)
set(TPW_UNIT_SetupBraceBezierCtrlParams_KEEP
  __Z13GenerateTrackP20tag_sCoasterInstance   # GenerateTrack(tag_sCoasterInstance*)
  __Z26SetupBraceBezierCtrlParamsP20tag_sCoasterInstance   # SetupBraceBezierCtrlParams(tag_sCoasterInstance*)
  _gBraceBezierMax
  _gBraceBezierMin
  _gTrackSubdivTimes
  _gTrackTextureCacheType
)
