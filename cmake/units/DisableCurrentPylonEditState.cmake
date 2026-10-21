# Unit v5084: the pylon edit states of the track editor (ExchangeTrackBraces, ToggleSwapUpPylonState, ToggleSwapDownPylonState,
# SetTrackPreview2ndBrace, DisableCurrentPylonEditState; src/game/rides/CoasterEdit.cpp). The callees outside the unit are recording
# fakes in tests/unit_DisableCurrentPylonEditState.cpp, which also defines the globals of other files.
set(TPW_UNIT_DisableCurrentPylonEditState src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_DisableCurrentPylonEditState_KEEP
  __Z19ExchangeTrackBracesP15tag_sTrackBraceS0_   # ExchangeTrackBraces(tag_sTrackBrace*, tag_sTrackBrace*)
  __Z22ToggleSwapUpPylonStatev   # ToggleSwapUpPylonState()
  __Z23SetTrackPreview2ndBraceP15tag_sTrackBracei   # SetTrackPreview2ndBrace(tag_sTrackBrace*, int)
  __Z24ToggleSwapDownPylonStatev   # ToggleSwapDownPylonState()
  __Z28DisableCurrentPylonEditStatev   # DisableCurrentPylonEditState()
  _gCoasterWord16DC
  _gCoasterWord1788
  _gCoasterWord1790
)
