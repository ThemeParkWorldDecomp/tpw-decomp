# Unit v5231: the ride list panel's row functions (src/game/ui/ui_researchPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_LCHandler.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCHandler src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_LCHandler_KEEP
  __Z16RideListLCFormatv   # RideListLCFormat()
  __Z17RideListLCHandlerP15InterfaceWindowiii   # RideListLCHandler(InterfaceWindow*, int, int, int)
  __Z20RideListLCPostFormatP15InterfaceWindow   # RideListLCPostFormat(InterfaceWindow*)
  __Z22RideListLCEntryHandlerP15InterfaceWindowiii   # RideListLCEntryHandler(InterfaceWindow*, int, int, int)
  _gRideListFontIndex
  _gRideListRowHeight
)
