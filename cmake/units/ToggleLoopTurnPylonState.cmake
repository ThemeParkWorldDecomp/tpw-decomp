# Unit v5144: the editor's pylon drag toggles (ToggleLoopTurnPylonState, ToggleRotatePylonState, ToggleBankInclinePylonState;
# src/HandleCoasterDataChange_ToggleLoopTurnPylonState.cpp). The callees outside the unit are recording fakes in
# tests/unit_ToggleLoopTurnPylonState.cpp, which also defines the globals of other files.
set(TPW_UNIT_ToggleLoopTurnPylonState src/HandleCoasterDataChange_ToggleLoopTurnPylonState.cpp platform/arena.cpp)
