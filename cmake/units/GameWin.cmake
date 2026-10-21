# The game window's message functions (unit w136): Engine_GameWinMsg and Engine_GameFirstPersonMsg. Every callee is a recording fake.
set(TPW_UNIT_GameWin src/game/ui/ui_main.cpp)
set(TPW_UNIT_GameWin_KEEP
  __Z17Engine_GameWinMsgP15InterfaceWindowiii   # Engine_GameWinMsg(InterfaceWindow*, int, int, int)
  __Z25Engine_GameFirstPersonMsgP15InterfaceWindowiii   # Engine_GameFirstPersonMsg(InterfaceWindow*, int, int, int)
  _gGameWinCameraOn
  _gGameWinCameraOnGuard
  _gGameWinCheat
  _gGameWinCheatGuard
  _gGameWinPointerIn
  _gGameWinPressTicks
  _gGameWinPressTicksGuard
  _gGameWinPressX
  _gGameWinPressY
  _gGameWinRightDown
  _gGameWinRightDownGuard
  _gGameWinWordC34
  _gGameWinWordC38
  _gRideHighlightKeep
)
