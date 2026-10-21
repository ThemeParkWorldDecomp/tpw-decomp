# Unit v5225: the online news panel (TextPanelMsg, CreateOnlineNewsPanel, CalcScrollBar; src/game/ui/ui_onlineNewsPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_TextPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_TextPanelMsg src/game/ui/ui_onlineNewsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_TextPanelMsg_KEEP
  __Z12TextPanelMsgP15InterfaceWindowiii   # TextPanelMsg(InterfaceWindow*, int, int, int)
  __Z13CalcScrollBarv   # CalcScrollBar()
  __Z21CreateOnlineNewsPanelv   # CreateOnlineNewsPanel()
  _gOnlineNewsFont
  _gOnlineNewsPanel
  _gOnlineNewsScript
  _gOnlineNewsShowInbox
)
