# Unit v5189: the city lobby controller (UpdateCityLobbyUI, CreateCityLobbyController, CityLobbyControllerMsg;
# src/game/ui/ui_cityLobbyController.cpp). The callees outside the unit are recording fakes in
# tests/unit_UpdateCityLobbyUI.cpp, which also defines the globals of other files (and stands in for swprintf and wcslen).
set(TPW_UNIT_UpdateCityLobbyUI src/game/ui/ui_cityLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_UpdateCityLobbyUI_KEEP
  __Z17UpdateCityLobbyUIP9CLBIsland   # UpdateCityLobbyUI(CLBIsland*)
  __Z22CityLobbyControllerMsgP15InterfaceWindowiii   # CityLobbyControllerMsg(InterfaceWindow*, int, int, int)
  __Z25CreateCityLobbyControllerv   # CreateCityLobbyController()
  _gCityLobbyScript
  _gCityLobbyWindow
)
