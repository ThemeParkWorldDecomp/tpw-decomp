# Unit v5540: CoasterPanelMsg (src/game/ui/ui_coasterPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CoasterPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterPanelMsg src/game/ui/ui_coasterPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterPanelMsg_KEEP
  __Z15CoasterPanelMsgP15InterfaceWindowiii   # CoasterPanelMsg(InterfaceWindow*, int, int, int)
)
