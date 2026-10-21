# Unit v5191: the ride status panel's UpdateStatusWindow, ShowUpgradePanel and UpdateWhichButtons (src/game/ui/ui_RideStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_RideStatusPanel_UpdateStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideStatusPanel_UpdateStatusWindow src/game/ui/ui_RideStatus.cpp platform/arena.cpp)
set(TPW_UNIT_RideStatusPanel_UpdateStatusWindow_KEEP
  __ZN15RideStatusPanel16ShowUpgradePanelEv   # RideStatusPanel::ShowUpgradePanel()
  __ZN15RideStatusPanel18UpdateStatusWindowEv   # RideStatusPanel::UpdateStatusWindow()
  __ZN15RideStatusPanel18UpdateWhichButtonsEv   # RideStatusPanel::UpdateWhichButtons()
  _gUpgradePanelScript
)
