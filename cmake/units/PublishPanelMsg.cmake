# Unit v5064: the publish park panel and the online park list panel (PublishPanelMsg, CreatePublishParkPanel, CreateOnlineParkListPanel;
# src/game/ui/ui_onlineParkListPanel.cpp, src/game/ui/ui_publishParkPanel.cpp). The callees outside the unit are recording fakes in tests/unit_PublishPanelMsg.cpp, which also defines the
# globals of other files. List only the sources the library must compile.
set(TPW_UNIT_PublishPanelMsg src/game/ui/ui_onlineParkListPanel.cpp src/game/ui/ui_publishParkPanel.cpp
    platform/arena.cpp)
set(TPW_UNIT_PublishPanelMsg_KEEP
  __Z15PublishPanelMsgP15InterfaceWindowiii   # PublishPanelMsg(InterfaceWindow*, int, int, int)
  __Z22CreatePublishParkPanelv   # CreatePublishParkPanel()
  __Z25CreateOnlineParkListPanelPFvvE   # CreateOnlineParkListPanel(void (*)())
  _gOnlineParkListClose
  _gOnlineParkListPanel
  _gOnlineParkListScript
  _gOnlineParkSelection
  _gOnlineUploadFile
  _gPublishParkDescription
  _gPublishParkName
  _gPublishParkPanel
  _gPublishParkPanelScript
)
