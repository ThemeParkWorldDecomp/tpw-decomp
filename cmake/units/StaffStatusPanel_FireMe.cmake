# Unit v5107: the staff status panel (FireMe, Show, InitWindow, UpdateViewWindow; src/game/ui/ui_staffStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_StaffStatusPanel_FireMe.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffStatusPanel_FireMe src/game/ui/ui_staffStatus.cpp platform/arena.cpp)
set(TPW_UNIT_StaffStatusPanel_FireMe_KEEP
  __ZN16StaffStatusPanel10InitWindowEv   # StaffStatusPanel::InitWindow()
  __ZN16StaffStatusPanel16UpdateViewWindowEv   # StaffStatusPanel::UpdateViewWindow()
  __ZN16StaffStatusPanel4ShowEP6CThing   # StaffStatusPanel::Show(CThing*)
  __ZN16StaffStatusPanel6FireMeEv   # StaffStatusPanel::FireMe()
  _gStaffStatusScript
)
