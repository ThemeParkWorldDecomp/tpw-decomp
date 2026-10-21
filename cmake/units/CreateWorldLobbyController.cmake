# Unit v5948: CreateWorldLobbyController and SatelliteMsg (src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_worldLobbyController.cpp). The callees
# outside the unit are recording fakes in tests/unit_CreateWorldLobbyController.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateWorldLobbyController src/game/ui/InterfaceTextObjectSprite.cpp
    src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_CreateWorldLobbyController_KEEP
  __Z12SatelliteMsgP15InterfaceWindowiii   # SatelliteMsg(InterfaceWindow*, int, int, int)
  __Z26CreateWorldLobbyControllerb   # CreateWorldLobbyController(bool)
  __ZN17worldlobby_callee13NewNumberTextEv   # worldlobby_callee::NewNumberText()
  _gWorldLobbyScript
)
