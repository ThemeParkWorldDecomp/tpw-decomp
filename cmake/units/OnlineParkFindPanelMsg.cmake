# Unit v5541: CreateOnlinePanel and OnlineParkFindPanelMsg (src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_OnlineParkFindPanelMsg.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_OnlineParkFindPanelMsg src/game/ui/ui_onlinePanel.cpp src/game/ui/ui_onlineParkFindPanel.cpp
    platform/arena.cpp)
set(TPW_UNIT_OnlineParkFindPanelMsg_KEEP
  __Z17CreateOnlinePanelv   # CreateOnlinePanel()
  __Z22OnlineParkFindPanelMsgP15InterfaceWindowiii   # OnlineParkFindPanelMsg(InterfaceWindow*, int, int, int)
  _gFindPanelCloseCallback
  _gFindPanelPlayerName
  _gFindPanelWord
  _gOnlineByte78
  _gOnlineEditFontIndex
  _gOnlineEditText
  _gOnlinePanelScript
  _gOnlineUnderlayScriptA
  _gOnlineUnderlayScriptB
  _gOnlineWord88
)
