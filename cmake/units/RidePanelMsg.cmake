# Unit v51005: RidePanelMsg (src/game/ui/ui_ridepanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_RidePanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_RidePanelMsg src/game/ui/ui_ridepanel.cpp platform/arena.cpp)
set(TPW_UNIT_RidePanelMsg_KEEP
  __Z12RidePanelMsgP15InterfaceWindowiii   # RidePanelMsg(InterfaceWindow*, int, int, int)
)
