# Unit v5100: the online city info overlay and the login panel's message function (src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp). The callees
# outside the unit are recording fakes in tests/unit_UpdateOnlineCityInfoOverlay.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateOnlineCityInfoOverlay src/game/ui/ui_onlineCityInfoOverlay.cpp src/game/ui/ui_onlineLogin.cpp
    platform/arena.cpp)
set(TPW_UNIT_UpdateOnlineCityInfoOverlay_KEEP
  __Z22OnlineLoginPanelDlgMsgP15InterfaceWindowiii   # OnlineLoginPanelDlgMsg(InterfaceWindow*, int, int, int)
  __Z27CreateOnlineCityInfoOverlayv   # CreateOnlineCityInfoOverlay()
  __Z27UpdateOnlineCityInfoOverlayPK13CFSOnlineCity   # UpdateOnlineCityInfoOverlay(CFSOnlineCity const*)
  _gOnlineCityOverlay
  _gOnlineCityOverlayShown
  _gOnlineLoginName
  _gOnlineLoginPanelFlag
  _gOnlineLoginPassword
  _kOnlineCityNoText
)
