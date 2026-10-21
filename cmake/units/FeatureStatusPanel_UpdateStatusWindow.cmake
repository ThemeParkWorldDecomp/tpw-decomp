# Unit v5311: the feature status panel (UpdateStatusWindow, InitWindow, UpdateWindow, RideIt, UpdateViewWindow, Show; src/game/ui/ui_FeatureStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_FeatureStatusPanel_UpdateStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_FeatureStatusPanel_UpdateStatusWindow src/game/ui/ui_FeatureStatus.cpp platform/arena.cpp)
set(TPW_UNIT_FeatureStatusPanel_UpdateStatusWindow_KEEP
  __ZN18FeatureStatusPanel10InitWindowEv   # FeatureStatusPanel::InitWindow()
  __ZN18FeatureStatusPanel12UpdateWindowEv   # FeatureStatusPanel::UpdateWindow()
  __ZN18FeatureStatusPanel16UpdateViewWindowEv   # FeatureStatusPanel::UpdateViewWindow()
  __ZN18FeatureStatusPanel18UpdateStatusWindowEv   # FeatureStatusPanel::UpdateStatusWindow()
  __ZN18FeatureStatusPanel4ShowEP6CThing   # FeatureStatusPanel::Show(CThing*)
  __ZN18FeatureStatusPanel6RideItEv   # FeatureStatusPanel::RideIt()
  _gFeatureStatusScript
  _gFeatureStatusThing
)
