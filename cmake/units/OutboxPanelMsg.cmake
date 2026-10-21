# Unit v5239: the online outbox panel (OutboxPanelMsg, SendAndDeleteOutbox; src/game/ui/ui_onlineOutbox.cpp).
# The callees outside the unit are recording fakes in tests/unit_OutboxPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_OutboxPanelMsg src/game/ui/ui_onlineOutbox.cpp platform/arena.cpp)
set(TPW_UNIT_OutboxPanelMsg_KEEP
  __Z14OutboxPanelMsgP15InterfaceWindowiii   # OutboxPanelMsg(InterfaceWindow*, int, int, int)
  __Z19SendAndDeleteOutboxb   # SendAndDeleteOutbox(bool)
  _gOutboxCallbackVector
)
