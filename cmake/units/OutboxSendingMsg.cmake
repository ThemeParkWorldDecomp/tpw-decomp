# Unit v5031: the outbox window's messages and the outbox marks (MarkOutboxEmail, SendMailStatusDisplay, OutboxSendingMsg;
# src/UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg.cpp). The callees outside the unit are recording fakes in
# tests/unit_OutboxSendingMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_OutboxSendingMsg src/UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg.cpp platform/arena.cpp)
