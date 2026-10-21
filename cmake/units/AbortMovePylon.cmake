# Unit v5967: AbortMovePylon and six small functions (src/game/park/TPHField.cpp, src/game/rides/CoasterEdit.cpp, src/game/rides/CoasterTrack.cpp). The callees outside the unit are recording fakes in
# tests/unit_AbortMovePylon.cpp, which also defines the globals of other files.
set(TPW_UNIT_AbortMovePylon src/game/park/TPHField.cpp src/game/rides/CoasterEdit.cpp src/game/rides/CoasterTrack.cpp
    platform/arena.cpp)
set(TPW_UNIT_AbortMovePylon_KEEP
  __Z10PlacePylonjjiPj   # PlacePylon(unsigned int, unsigned int, int, unsigned int*)
  __Z12IsTrackValidPv   # IsTrackValid(void*)
  __Z13IsTrackClosedPv   # IsTrackClosed(void*)
  __Z14AbortMovePylonv   # AbortMovePylon()
  __Z16DecCoasterConst2v   # DecCoasterConst2()
  __Z16IsCoasterUseableP7CObject   # IsCoasterUseable(CObject*)
  __Z25SingleHFieldTextureChangeiiPKN9CCellBase14CELL_IndexTypeE   # SingleHFieldTextureChange(int, int, CCellBase::CELL_IndexType const*)
)
