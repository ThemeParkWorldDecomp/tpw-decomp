# Unit v5031: the outbox window's messages and the outbox marks (MarkOutboxEmail, SendMailStatusDisplay, OutboxSendingMsg;
# src/game/ui/ui_onlineOutbox.cpp). The callees outside the unit are recording fakes in
# tests/unit_OutboxSendingMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_OutboxSendingMsg src/game/ui/ui_onlineOutbox.cpp platform/arena.cpp)
set(TPW_UNIT_OutboxSendingMsg_KEEP
  __Z15MarkOutboxEmaili   # MarkOutboxEmail(int)
  __Z16OutboxSendingMsgP15InterfaceWindowiii   # OutboxSendingMsg(InterfaceWindow*, int, int, int)
  __Z21SendMailStatusDisplayv   # SendMailStatusDisplay()
  _gMailStatusA
  _gMailStatusB
  _gMailStatusShownA
  _gMailStatusShownB
  _gOutboxSendingWindow
)
