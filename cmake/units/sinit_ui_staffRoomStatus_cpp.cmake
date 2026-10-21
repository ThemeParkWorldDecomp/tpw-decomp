# Unit v5951: the staff room / show status static initialisers, StaffRoomListFiller, StaffRoomLCFormat, StaffStatusViewWindowCallback and
# StaffStatusPanel::ShowEditNamePanel (src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffRoomStatus.cpp, src/game/ui/ui_staffStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_sinit_ui_staffRoomStatus_cpp.cpp, which also defines the globals of other files.
set(TPW_UNIT_sinit_ui_staffRoomStatus_cpp src/game/ui/ui_showStatus.cpp src/game/ui/ui_staffRoomStatus.cpp
    src/game/ui/ui_staffStatus.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_ui_staffRoomStatus_cpp_KEEP
  __Z17StaffRoomLCFormatv   # StaffRoomLCFormat()
  __Z19StaffRoomListFillerP6CThing   # StaffRoomListFiller(CThing*)
  __Z25__sinit_ui_showStatus_cppv   # __sinit_ui_showStatus_cpp()
  __Z29StaffStatusViewWindowCallbackP15InterfaceWindowiii   # StaffStatusViewWindowCallback(InterfaceWindow*, int, int, int)
  __Z30__sinit_ui_staffRoomStatus_cppv   # __sinit_ui_staffRoomStatus_cpp()
  __ZN16StaffStatusPanel17ShowEditNamePanelEv   # StaffStatusPanel::ShowEditNamePanel()
  __ZN16staffroom_callee10FontHeightEP13InterfaceFont   # staffroom_callee::FontHeight(InterfaceFont*)
  __ZN16staffroom_callee12NewRowWindowEs   # staffroom_callee::NewRowWindow(short)
  __ZN16staffroom_callee12NewTitleTextEv   # staffroom_callee::NewTitleText()
  __ZN16staffroom_callee13NewCursorTextEv   # staffroom_callee::NewCursorText()
  __ZN16staffroom_callee13WindowSetTextEP15InterfaceWindowPKDs   # staffroom_callee::WindowSetText(InterfaceWindow*, char16_t const*)
  _gShowStatusTable
  _gStaffEditNameScript
  _gStaffRoomListFormat
  _gStaffRoomPanelTable
  _gStaffRoomValueA
  _gStaffRoomValueB
)
