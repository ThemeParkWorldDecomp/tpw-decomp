# Unit v5550: the online park info panel's messages and the park find panel (src/game/ui/ui_onlineParkFindPanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp). The callees outside the unit
# are recording fakes in tests/unit_OnlineParkInfoPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_OnlineParkInfoPanelMsg src/game/ui/ui_onlineParkFindPanel.cpp src/game/ui/ui_onlineParkInfoPanel.cpp
    platform/arena.cpp)
set(TPW_UNIT_OnlineParkInfoPanelMsg_KEEP
  __Z22OnlineParkInfoPanelMsgP15InterfaceWindowiii   # OnlineParkInfoPanelMsg(InterfaceWindow*, int, int, int)
  __Z25RemoveOnlineParkFindPanelv   # RemoveOnlineParkFindPanel()
  __Z27UpdateParkFindPanelProgressi   # UpdateParkFindPanelProgress(int)
  __Z31VisitParkPostDownloadCallbackFniPKv   # VisitParkPostDownloadCallbackFn(int, void const*)
  __Z34__sinit_ui_onlineParkFindPanel_cppv   # __sinit_ui_onlineParkFindPanel_cpp()
  __Z35GeneralParkDownloadCompleteCallbackiPKv   # GeneralParkDownloadCompleteCallback(int, void const*)
  __ZN15parkinfo_callee19DownloadGlobeSlot4cEPv   # parkinfo_callee::DownloadGlobeSlot4c(void*)
  __ZN15parkinfo_callee19OnlineIslandsSlot30EPv   # parkinfo_callee::OnlineIslandsSlot30(void*)
  _gOnlineParkFindNodeA
  _gOnlineParkFindNodeB
  _gOnlineParkFindPanel
  _gOnlineParkFindStringA
  _gOnlineParkFindStringB
  _gOnlineParkInfoClose
  _gOnlineParkInfoPanel
  _gOnlineParkInfoParkStorage
  _gVisitParkContext
  _gVisitParkPostDownload
)
