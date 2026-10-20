# Unit v5084: the pylon edit states of the track editor (ExchangeTrackBraces, ToggleSwapUpPylonState, ToggleSwapDownPylonState,
# SetTrackPreview2ndBrace, DisableCurrentPylonEditState; src/DisableCurrentPylonEditState.cpp). The callees outside the unit are recording
# fakes in tests/unit_DisableCurrentPylonEditState.cpp, which also defines the globals of other files.
set(TPW_UNIT_DisableCurrentPylonEditState src/DisableCurrentPylonEditState.cpp platform/arena.cpp)
