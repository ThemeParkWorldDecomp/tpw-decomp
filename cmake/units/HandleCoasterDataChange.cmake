# Unit v5054: HandleCoasterDataChange and ToggleLoftPylonState (src/game/rides/CoasterEdit.cpp, src/game/rides/CoasterTrack.cpp). The callees outside the unit are recording fakes in
# tests/unit_HandleCoasterDataChange.cpp, which also defines the globals of other files.
set(TPW_UNIT_HandleCoasterDataChange src/game/rides/CoasterEdit.cpp src/game/rides/CoasterTrack.cpp platform/arena.cpp)
set(TPW_UNIT_HandleCoasterDataChange_KEEP
  __Z20ToggleLoftPylonStatev   # ToggleLoftPylonState()
  __Z23HandleCoasterDataChangejP15tag_sTrackBraceP20tag_sCoasterInstanceii   # HandleCoasterDataChange(unsigned int, tag_sTrackBrace*, tag_sCoasterInstance*, int, int)
  _gCoasterVerticalFloor
)
