# Unit v51011: RideStatusPanel::InitWindow, UpdateWindow, InitKnobsWindow, UpdateKnobsTitles
# (src/game/ui/ui_RideStatus.cpp). The callees outside the unit
# are recording fakes in tests/unit_RideStatusPanel_InitKnobsWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideStatusPanel_InitKnobsWindow src/game/ui/ui_RideStatus.cpp platform/arena.cpp)
set(TPW_UNIT_RideStatusPanel_InitKnobsWindow_KEEP
  __ZN15RideStatusPanel10InitWindowEv   # RideStatusPanel::InitWindow()
  __ZN15RideStatusPanel12UpdateWindowEv   # RideStatusPanel::UpdateWindow()
  __ZN15RideStatusPanel15InitKnobsWindowEv   # RideStatusPanel::InitKnobsWindow()
  __ZN15RideStatusPanel17UpdateKnobsTitlesEi   # RideStatusPanel::UpdateKnobsTitles(int)
  _gRideStatusMeshNames
)
