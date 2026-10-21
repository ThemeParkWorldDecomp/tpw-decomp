# Unit v5019: the all-staff panel (CreateAllStaffPanel; src/game/ui/ui_allStaffPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateAllStaffPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateAllStaffPanel src/game/ui/ui_allStaffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateAllStaffPanel_KEEP
  __Z19CreateAllStaffPanel9StaffTypeP6CThing   # CreateAllStaffPanel(StaffType, CThing*)
  _gStaffColumnFont
  _gStaffPanel
  _gStaffSort
  _gStaffType
)
