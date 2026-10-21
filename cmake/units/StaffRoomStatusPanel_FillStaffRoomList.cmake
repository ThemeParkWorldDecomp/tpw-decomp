# Unit v5669: the staff room panel's list fill and window set-up (FillStaffRoomList, InitStatusWindow, InitWindow, UpdateViewSprite;
# src/game/ui/ui_staffRoomStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_StaffRoomStatusPanel_FillStaffRoomList.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffRoomStatusPanel_FillStaffRoomList src/game/ui/ui_staffRoomStatus.cpp platform/arena.cpp
    tests/standins/StaffRoomStatusPanel_FillStaffRoomList.cpp)
set(TPW_UNIT_StaffRoomStatusPanel_FillStaffRoomList_KEEP
  __ZN20StaffRoomStatusPanel10InitWindowEv   # StaffRoomStatusPanel::InitWindow()
  __ZN20StaffRoomStatusPanel16InitStatusWindowEv   # StaffRoomStatusPanel::InitStatusWindow()
  __ZN20StaffRoomStatusPanel16UpdateViewSpriteEP6CStaff   # StaffRoomStatusPanel::UpdateViewSprite(CStaff*)
  __ZN20StaffRoomStatusPanel17FillStaffRoomListE9StaffTypei   # StaffRoomStatusPanel::FillStaffRoomList(StaffType, int)
  _gStaffRoomColumnColour
  _gStaffRoomList
  _gStaffRoomSpawnBlock
)
