# Unit v5257: SetCoasterEditState, RejoinTrack, SetPlaceLoop (src/game/rides/CoasterEdit.cpp). The callees outside the unit are
# recording fakes in tests/unit_SetCoasterEditState.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetCoasterEditState src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_SetCoasterEditState_KEEP
  __Z11RejoinTrackP20tag_sCoasterInstance   # RejoinTrack(tag_sCoasterInstance*)
  __Z12SetPlaceLoopv   # SetPlaceLoop()
  __Z19SetCoasterEditStatej   # SetCoasterEditState(unsigned int)
)
