# Unit v5119: InterfaceWindow leaves (src/game/ui/InterfaceWindow.cpp). The callees outside the unit are
# recording fakes in tests/unit_InterfaceWindow_GetSizeOfClientDrawObject.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceWindow_GetSizeOfClientDrawObject src/game/ui/InterfaceWindow.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceWindow_GetSizeOfClientDrawObject_KEEP
  __ZN15InterfaceWindow11UnCaptureKbEi   # InterfaceWindow::UnCaptureKb(int)
  __ZN15InterfaceWindow12setAnimTimerEii   # InterfaceWindow::setAnimTimer(int, int)
  __ZN15InterfaceWindow14AdjustPositionEssss   # InterfaceWindow::AdjustPosition(short, short, short, short)
  __ZN15InterfaceWindow17SetWindowDrawTextEPDs   # InterfaceWindow::SetWindowDrawText(char16_t*)
  __ZN15InterfaceWindow25GetSizeOfClientDrawObjectEP8CoolRect   # InterfaceWindow::GetSizeOfClientDrawObject(CoolRect*)
  __ZN15InterfaceWindow8stopAnimEv   # InterfaceWindow::stopAnim()
)
