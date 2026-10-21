# Unit v5207: the staff status panel's InitStatusWindow, UpdateStatusWindow and UpdateWindow
# (src/game/ui/ui_staffStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_StaffStatusPanel_UpdateStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffStatusPanel_UpdateStatusWindow src/game/ui/ui_staffStatus.cpp platform/arena.cpp)
set(TPW_UNIT_StaffStatusPanel_UpdateStatusWindow_KEEP
  __ZN16StaffStatusPanel12UpdateWindowEv   # StaffStatusPanel::UpdateWindow()
  __ZN16StaffStatusPanel16InitStatusWindowEv   # StaffStatusPanel::InitStatusWindow()
  __ZN16StaffStatusPanel18UpdateStatusWindowEv   # StaffStatusPanel::UpdateStatusWindow()
)
