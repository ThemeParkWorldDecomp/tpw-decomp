# Unit v5522: the game and lobby menus' handlers (MainGameMenuConfirmPublish_OK, MainGameMenuCB, MainLobbyMenuConfirmLogout_OK,
# MainLobbyMenuCB; src/game/ui/ui_mainmenu.cpp). The callees outside the unit are recording fakes in
# tests/unit_MainGameMenuConfirmPublish_OK.cpp, which also defines the globals of other files.
set(TPW_UNIT_MainGameMenuConfirmPublish_OK src/game/ui/ui_mainmenu.cpp platform/arena.cpp)
set(TPW_UNIT_MainGameMenuConfirmPublish_OK_KEEP
  __Z14MainGameMenuCBP15InterfaceWindowiii   # MainGameMenuCB(InterfaceWindow*, int, int, int)
  __Z15MainLobbyMenuCBP15InterfaceWindowiii   # MainLobbyMenuCB(InterfaceWindow*, int, int, int)
  __Z29MainGameMenuConfirmPublish_OKPv   # MainGameMenuConfirmPublish_OK(void*)
  __Z29MainLobbyMenuConfirmLogout_OKPv   # MainLobbyMenuConfirmLogout_OK(void*)
  __ZN15mainmenu_callee10ScreenHideEPv   # mainmenu_callee::ScreenHide(void*)
  __ZN15mainmenu_callee10ScreenShowEPv   # mainmenu_callee::ScreenShow(void*)
  __ZN15mainmenu_callee11NewUserInfoEv   # mainmenu_callee::NewUserInfo()
  __ZN15mainmenu_callee15NewOnlineSystemEv   # mainmenu_callee::NewOnlineSystem()
  __ZN15mainmenu_callee15ScreenIsVisibleEPv   # mainmenu_callee::ScreenIsVisible(void*)
  __ZN15mainmenu_callee18PublishGlobeSlot34EPv   # mainmenu_callee::PublishGlobeSlot34(void*)
)
