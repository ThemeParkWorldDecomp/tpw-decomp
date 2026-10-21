# Unit v5142: the park status panel (UI_ParkStatusPanelDateChange, __sinit_ui_parkStatus_cpp; src/game/ui/ui_parkStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_sinit_ui_parkStatus_cpp.cpp.
set(TPW_UNIT_sinit_ui_parkStatus_cpp src/game/ui/ui_parkStatus.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_ui_parkStatus_cpp_KEEP
  __Z25__sinit_ui_parkStatus_cppv   # __sinit_ui_parkStatus_cpp()
  __Z28UI_ParkStatusPanelDateChangei   # UI_ParkStatusPanelDateChange(int)
  _gParkStatusColours
  _gParkStatusFlags
  _gParkStatusGrapher
  _gParkStatusPanel
  _gParkStatusRange
)
