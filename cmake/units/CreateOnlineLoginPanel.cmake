# Unit v5339: CreateOnlineLoginPanel (src/game/ui/ui_onlineLogin.cpp). The callees outside the unit are
# recording fakes in tests/unit_CreateOnlineLoginPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateOnlineLoginPanel src/game/ui/ui_onlineLogin.cpp platform/arena.cpp)
set(TPW_UNIT_CreateOnlineLoginPanel_KEEP
  __Z22CreateOnlineLoginPanelPFvvE   # CreateOnlineLoginPanel(void (*)())
  _gOnlineLoginEmptyText
  _gOnlineLoginFailureFn
  _gOnlineLoginPanelScript
)
