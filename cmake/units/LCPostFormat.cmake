# Unit v5004: the staff room panel's list formatter and message functions (src/game/ui/ui_staffRoomStatus.cpp). The callees outside the unit are recording
# fakes in tests/unit_LCPostFormat.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCPostFormat src/game/ui/ui_staffRoomStatus.cpp platform/arena.cpp)
set(TPW_UNIT_LCPostFormat_KEEP
  __Z12LCPostFormatP15InterfaceWindow   # LCPostFormat(InterfaceWindow*)
  __Z18StaffRoomStatusMsgP15InterfaceWindowiii   # StaffRoomStatusMsg(InterfaceWindow*, int, int, int)
  __Z23StaffRoomPanelLCHandlerP15InterfaceWindowiii   # StaffRoomPanelLCHandler(InterfaceWindow*, int, int, int)
)
