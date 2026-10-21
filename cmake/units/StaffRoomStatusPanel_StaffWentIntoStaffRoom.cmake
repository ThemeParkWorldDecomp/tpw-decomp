# Unit v5291: the staff room panel's member functions (src/game/ui/ui_staffRoomStatus.cpp). The callees outside
# the unit are recording fakes in tests/unit_StaffRoomStatusPanel_StaffWentIntoStaffRoom.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffRoomStatusPanel_StaffWentIntoStaffRoom src/game/ui/ui_staffRoomStatus.cpp platform/arena.cpp)
set(TPW_UNIT_StaffRoomStatusPanel_StaffWentIntoStaffRoom_KEEP
  __ZN20StaffRoomStatusPanel12UpdateWindowEv   # StaffRoomStatusPanel::UpdateWindow()
  __ZN20StaffRoomStatusPanel16DeInitViewWindowEv   # StaffRoomStatusPanel::DeInitViewWindow()
  __ZN20StaffRoomStatusPanel18StaffLeftStaffRoomEP6CThing   # StaffRoomStatusPanel::StaffLeftStaffRoom(CThing*)
  __ZN20StaffRoomStatusPanel18UpdateStatusWindowEv   # StaffRoomStatusPanel::UpdateStatusWindow()
  __ZN20StaffRoomStatusPanel22StaffWentIntoStaffRoomEP6CThing   # StaffRoomStatusPanel::StaffWentIntoStaffRoom(CThing*)
  __ZN20StaffRoomStatusPanel4ShowEP6CThing   # StaffRoomStatusPanel::Show(CThing*)
  _gStaffRoomId
  _gStaffRoomScript
  _gStaffRoomShowThing
)
