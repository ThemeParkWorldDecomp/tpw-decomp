# Unit v5096: the online outbox panel (CreateOnlineOutboxPanel and its row functions; src/game/ui/ui_onlineOutbox.cpp). The callees outside the unit
# are recording fakes in tests/unit_LCPostFormat_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCPostFormat_2 src/game/ui/ui_onlineOutbox.cpp platform/arena.cpp)
set(TPW_UNIT_LCPostFormat_2_KEEP
  __Z14OutboxLCFormatv   # OutboxLCFormat()
  __Z18OutboxLCPostFormatP15InterfaceWindow   # OutboxLCPostFormat(InterfaceWindow*)
  __Z23CreateOnlineOutboxPanelv   # CreateOnlineOutboxPanel()
)
