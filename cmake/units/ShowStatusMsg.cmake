# Unit v5525: ShowStatusMsg (src/game/ui/ui_showStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_ShowStatusMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowStatusMsg src/game/ui/ui_showStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShowStatusMsg_KEEP
  __Z13ShowStatusMsgP15InterfaceWindowiii   # ShowStatusMsg(InterfaceWindow*, int, int, int)
)
