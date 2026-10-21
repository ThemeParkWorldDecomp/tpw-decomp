# Unit v5144: the editor's pylon drag toggles (ToggleLoopTurnPylonState, ToggleRotatePylonState, ToggleBankInclinePylonState;
# src/game/rides/CoasterEdit.cpp). The callees outside the unit are recording fakes in
# tests/unit_ToggleLoopTurnPylonState.cpp, which also defines the globals of other files.
set(TPW_UNIT_ToggleLoopTurnPylonState src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_ToggleLoopTurnPylonState_KEEP
  __Z22ToggleRotatePylonStatev   # ToggleRotatePylonState()
  __Z24ToggleLoopTurnPylonStatev   # ToggleLoopTurnPylonState()
  __Z27ToggleBankInclinePylonStatev   # ToggleBankInclinePylonState()
)
