# Unit v5910: the status panel's handlers (src/game/ui/ui_StatusPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_DefaultStatusPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_DefaultStatusPanelMsg src/game/ui/ui_StatusPanel.cpp platform/arena.cpp)
set(TPW_UNIT_DefaultStatusPanelMsg_KEEP
  __Z17TwoEditsMsgStatusP15InterfaceWindowiii   # TwoEditsMsgStatus(InterfaceWindow*, int, int, int)
  __Z21DefaultStatusPanelMsgP15InterfaceWindowiii   # DefaultStatusPanelMsg(InterfaceWindow*, int, int, int)
  __ZN13UIStatusPanel11InitEditBoxEi   # UIStatusPanel::InitEditBox(int)
  __ZN13UIStatusPanel17DisplayStatusTextEv   # UIStatusPanel::DisplayStatusText()
  _gStatusPanelEmptyText
)
