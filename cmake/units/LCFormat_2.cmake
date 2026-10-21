# Unit v5153: the all-staff list's row functions and the panel handler (src/game/ui/ui_allStaffPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_LCFormat_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCFormat_2 src/game/ui/ui_allStaffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_LCFormat_2_KEEP
  __Z16AllStaffLCFormatv   # AllStaffLCFormat()
  __Z20AllStaffLCPostFormatP15InterfaceWindow   # AllStaffLCPostFormat(InterfaceWindow*)
  __Z22AllStaffPanelLCHandlerP15InterfaceWindowiii   # AllStaffPanelLCHandler(InterfaceWindow*, int, int, int)
  _gStaffListFont
  _gStaffListFormat
)
