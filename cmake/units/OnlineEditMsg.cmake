# Unit v5658: OnlineEditMsg and OnlinePanelSubPanelMsg (src/game/ui/ui_onlinePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_OnlineEditMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_OnlineEditMsg src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineEditMsg_KEEP
  __Z13OnlineEditMsgP15InterfaceWindowiii   # OnlineEditMsg(InterfaceWindow*, int, int, int)
  __Z22OnlinePanelSubPanelMsgP15InterfaceWindowiii   # OnlinePanelSubPanelMsg(InterfaceWindow*, int, int, int)
  __ZN18online_edit_callee11MouseModeOfEP10CMouseMode   # online_edit_callee::MouseModeOf(CMouseMode*)
  __ZN18online_edit_callee13WindowSetTextEP15InterfaceWindowPDs   # online_edit_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN18online_edit_callee3NewEm   # online_edit_callee::New(unsigned long)
)
