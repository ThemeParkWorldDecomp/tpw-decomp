# Unit v51067: the small online UI functions (src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp, src/game/ui/ui_onlineNewsPanel.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp). The callees outside the unit
# are recording fakes in tests/unit_OnlineParkListPostGetAllParksInCityCallbackFn.cpp, which also defines the globals of other files.
set(TPW_UNIT_OnlineParkListPostGetAllParksInCityCallbackFn src/game/ui/ui_cityLobbyController.cpp
    src/game/ui/ui_onlineCityInfoOverlay.cpp src/game/ui/ui_onlineLogin.cpp src/game/ui/ui_onlineNewsPanel.cpp
    src/game/ui/ui_onlineOutbox.cpp src/game/ui/ui_onlinePanel.cpp src/game/ui/ui_onlineParkFindPanel.cpp
    src/game/ui/ui_onlineParkInfoPanel.cpp src/game/ui/ui_onlineParkListPanel.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineParkListPostGetAllParksInCityCallbackFn_KEEP
  __Z14OutboxCallback19EmailCompletionCode14EmailErrorCode   # OutboxCallback(EmailCompletionCode, EmailErrorCode)
  __Z15FillContactListv   # FillContactList()
  __Z16MessagesLCFormatv   # MessagesLCFormat()
  __Z17RemoveOnlinePanelv   # RemoveOnlinePanel()
  __Z18CityInfoOverlayMsgP15InterfaceWindowiii   # CityInfoOverlayMsg(InterfaceWindow*, int, int, int)
  __Z20MessagesLCPostFormatP15InterfaceWindow   # MessagesLCPostFormat(InterfaceWindow*)
  __Z21RemoveOnlineNewsPanelv   # RemoveOnlineNewsPanel()
  __Z23HideCityLobbyControllerv   # HideCityLobbyController()
  __Z23ShowCityLobbyControllerv   # ShowCityLobbyController()
  __Z25HideOnlineCityInfoOverlayv   # HideOnlineCityInfoOverlay()
  __Z25RemoveCityLobbyControllerv   # RemoveCityLobbyController()
  __Z25RemoveOnlineParkInfoPanelv   # RemoveOnlineParkInfoPanel()
  __Z25ShowOnlineCityInfoOverlayv   # ShowOnlineCityInfoOverlay()
  __Z26__sinit_ui_onlineLogin_cppv   # __sinit_ui_onlineLogin_cpp()
  __Z27RemoveOnlineCityInfoOverlayv   # RemoveOnlineCityInfoOverlay()
  __Z30__sinit_ui_onlineNewsPanel_cppv   # __sinit_ui_onlineNewsPanel_cpp()
  __Z36__sinit_ui_onlineCityInfoOverlay_cppv   # __sinit_ui_onlineCityInfoOverlay_cpp()
  __Z37FindParksPostGetPlayerParksCallbackFniPKvP9TbPtrListI13CFSOnlineParkE   # FindParksPostGetPlayerParksCallbackFn(int, void const*, TbPtrList<CFSOnlinePark>*)
  __Z41ListParkOnlineParkInfoPanelCancelCallbackv   # ListParkOnlineParkInfoPanelCancelCallback()
  __Z45OnlineParkListPostGetAllParksInCityCallbackFniPKvP9TbPtrListI13CFSOnlineParkE   # OnlineParkListPostGetAllParksInCityCallbackFn(int, void const*, TbPtrList<CFSOnlinePark>*)
  _gOnlineCityInfoNodeA
  _gOnlineCityInfoStringA
  _gOnlineLoginNodeA
  _gOnlineLoginStringA
  _gOnlineNewsNodeA
  _gOnlineNewsStringA
  _gOnlineNewsText
)
