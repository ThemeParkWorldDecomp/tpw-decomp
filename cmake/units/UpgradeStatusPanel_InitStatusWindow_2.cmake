# Unit v5905: UpgradeStatusPanel InitStatusWindow, UpdateStatusWindow, InitWindow, Show (src/game/ui/ui_upgradeStatus.cpp).
set(TPW_UNIT_UpgradeStatusPanel_InitStatusWindow_2 src/game/ui/ui_upgradeStatus.cpp platform/arena.cpp)
set(TPW_UNIT_UpgradeStatusPanel_InitStatusWindow_2_KEEP
  __ZN18UpgradeStatusPanel10InitWindowEv   # UpgradeStatusPanel::InitWindow()
  __ZN18UpgradeStatusPanel16InitStatusWindowEv   # UpgradeStatusPanel::InitStatusWindow()
  __ZN18UpgradeStatusPanel18UpdateStatusWindowEv   # UpgradeStatusPanel::UpdateStatusWindow()
  __ZN18UpgradeStatusPanel4ShowEP6CThing   # UpgradeStatusPanel::Show(CThing*)
  _gUpgradeStatusScript
  _gUpgradeStatusThing
)
