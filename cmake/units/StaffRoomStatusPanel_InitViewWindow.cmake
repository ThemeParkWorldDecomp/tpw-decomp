# Unit v51051: the staff, show and upgrade status panels' view-window functions and the staff panel's small free functions
# (src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffPanel.cpp, src/game/ui/ui_staffRoomStatus.cpp, src/game/ui/ui_staffStatus.cpp, src/game/ui/ui_upgradeStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_StaffRoomStatusPanel_InitViewWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffRoomStatusPanel_InitViewWindow src/game/ui/ui_showStatus.cpp src/game/ui/ui_staffPanel.cpp
    src/game/ui/ui_staffRoomStatus.cpp src/game/ui/ui_staffStatus.cpp src/game/ui/ui_upgradeStatus.cpp
    platform/arena.cpp)
set(TPW_UNIT_StaffRoomStatusPanel_InitViewWindow_KEEP
  __Z11_applytoallP6CThing   # _applytoall(CThing*)
  __Z12StaffPanelUpv   # StaffPanelUp()
  __Z16RemoveStaffPanelv   # RemoveStaffPanel()
  __Z25__sinit_ui_staffPanel_cppv   # __sinit_ui_staffPanel_cpp()
  __Z6firemePv   # fireme(void*)
  __ZN15ShowStatusPanel12DeInitWindowEv   # ShowStatusPanel::DeInitWindow()
  __ZN15ShowStatusPanel14InitViewWindowEv   # ShowStatusPanel::InitViewWindow()
  __ZN15ShowStatusPanel16DeInitViewWindowEv   # ShowStatusPanel::DeInitViewWindow()
  __ZN15ShowStatusPanel16UpdateViewWindowEv   # ShowStatusPanel::UpdateViewWindow()
  __ZN15ShowStatusPanel18DeInitStatusWindowEv   # ShowStatusPanel::DeInitStatusWindow()
  __ZN15ShowStatusPanel6RemoveEv   # ShowStatusPanel::Remove()
  __ZN16StaffStatusPanel10ChangeNameEPDs   # StaffStatusPanel::ChangeName(char16_t*)
  __ZN16StaffStatusPanel11SetSelectedEv   # StaffStatusPanel::SetSelected()
  __ZN16StaffStatusPanel12DeInitWindowEv   # StaffStatusPanel::DeInitWindow()
  __ZN16StaffStatusPanel14InitViewWindowEv   # StaffStatusPanel::InitViewWindow()
  __ZN16StaffStatusPanel16DeInitViewWindowEv   # StaffStatusPanel::DeInitViewWindow()
  __ZN16StaffStatusPanel18DeInitStatusWindowEv   # StaffStatusPanel::DeInitStatusWindow()
  __ZN16StaffStatusPanel6RemoveEv   # StaffStatusPanel::Remove()
  __ZN16StaffStatusPanel7GetNameEv   # StaffStatusPanel::GetName()
  __ZN18UpgradeStatusPanel14InitViewWindowEv   # UpgradeStatusPanel::InitViewWindow()
  __ZN18UpgradeStatusPanel16DeInitViewWindowEv   # UpgradeStatusPanel::DeInitViewWindow()
  __ZN18UpgradeStatusPanel16UpdateViewWindowEv   # UpgradeStatusPanel::UpdateViewWindow()
  __ZN20StaffRoomStatusPanel12DeInitWindowEv   # StaffRoomStatusPanel::DeInitWindow()
  __ZN20StaffRoomStatusPanel14InitViewWindowEv   # StaffRoomStatusPanel::InitViewWindow()
  __ZN20StaffRoomStatusPanel16UpdateViewWindowEv   # StaffRoomStatusPanel::UpdateViewWindow()
  __ZN20StaffRoomStatusPanel18DeInitStatusWindowEv   # StaffRoomStatusPanel::DeInitStatusWindow()
  __ZN20StaffRoomStatusPanel6RemoveEv   # StaffRoomStatusPanel::Remove()
  _gStaffPanelWord4c
  _gStaffPanelWord50
)
