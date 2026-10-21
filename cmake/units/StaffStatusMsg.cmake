# Unit v5364: the staff status panel's message functions (StaffStatusMsg, OneEditMsg; src/game/ui/ui_staffStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_StaffStatusMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_StaffStatusMsg src/game/ui/ui_staffStatus.cpp platform/arena.cpp)
set(TPW_UNIT_StaffStatusMsg_KEEP
  __Z10OneEditMsgP15InterfaceWindowiii   # OneEditMsg(InterfaceWindow*, int, int, int)
  __Z14StaffStatusMsgP15InterfaceWindowiii   # StaffStatusMsg(InterfaceWindow*, int, int, int)
)
