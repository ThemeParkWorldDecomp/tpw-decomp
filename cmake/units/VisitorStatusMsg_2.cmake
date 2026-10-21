# Unit v5929: the visitor status panel (Show, InitWindow, UpdateWindow, VisitorStatusMsg, __sinit_ui_visitorStatus_cpp;
# src/game/ui/ui_visitorStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_VisitorStatusMsg_2.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_VisitorStatusMsg_2 src/game/ui/ui_visitorStatus.cpp platform/arena.cpp)
set(TPW_UNIT_VisitorStatusMsg_2_KEEP
  __Z16VisitorStatusMsgP15InterfaceWindowiii   # VisitorStatusMsg(InterfaceWindow*, int, int, int)
  __Z28__sinit_ui_visitorStatus_cppv   # __sinit_ui_visitorStatus_cpp()
  __ZN18VisitorStatusPanel10InitWindowEv   # VisitorStatusPanel::InitWindow()
  __ZN18VisitorStatusPanel12UpdateWindowEv   # VisitorStatusPanel::UpdateWindow()
  __ZN18VisitorStatusPanel4ShowEP6CThing   # VisitorStatusPanel::Show(CThing*)
  _gVisitorStatusScript
  _gVisitorStatusTable
  _gVisitorStatusThing
  _gVisitorStatusWordEC3C
  _gVisitorStatusWordEC40
)
