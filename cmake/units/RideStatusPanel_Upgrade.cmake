# Unit v5914: the ride status panel's knobs and buttons (Upgrade, ApplyChanges, DoDial, Edit, Show, UpdateKnobsWindow;
# src/game/ui/ui_RideStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_RideStatusPanel_Upgrade.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideStatusPanel_Upgrade src/game/ui/ui_RideStatus.cpp platform/arena.cpp)
set(TPW_UNIT_RideStatusPanel_Upgrade_KEEP
  __ZN15RideStatusPanel12ApplyChangesEv   # RideStatusPanel::ApplyChanges()
  __ZN15RideStatusPanel17UpdateKnobsWindowEv   # RideStatusPanel::UpdateKnobsWindow()
  __ZN15RideStatusPanel4EditEi   # RideStatusPanel::Edit(int)
  __ZN15RideStatusPanel4ShowEP6CThing   # RideStatusPanel::Show(CThing*)
  __ZN15RideStatusPanel6DoDialEii   # RideStatusPanel::DoDial(int, int)
  __ZN15RideStatusPanel7UpgradeEib   # RideStatusPanel::Upgrade(int, bool)
  __ZN20allitems_post_callee14WindowSetValueEP15InterfaceWindowi   # allitems_post_callee::WindowSetValue(InterfaceWindow*, int)
  _gRideStatusScript
  _gRideStatusShowThing
)
