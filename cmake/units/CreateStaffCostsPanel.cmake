# Unit v5088: CreateStaffCostsPanel (src/game/ui/ui_StaffCostsPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateStaffCostsPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateStaffCostsPanel src/game/ui/ui_StaffCostsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateStaffCostsPanel_KEEP
  __Z21CreateStaffCostsPanelv   # CreateStaffCostsPanel()
  _gStaffCostsDeposits
  _gStaffCostsPanel
  _gStaffCostsScript
  _gStaffCostsWithdrawals
  _gStaffCostsWord10
  _gStaffCostsWord14
)
