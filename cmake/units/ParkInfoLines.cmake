# Unit v5232: the park status panel (ParkInfoLines, ParkStatusPanelMsg, DoDominantThoughts; src/game/ui/ui_parkStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_ParkInfoLines.cpp, which also defines the globals of other files.
set(TPW_UNIT_ParkInfoLines src/game/ui/ui_parkStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ParkInfoLines_KEEP
  __Z13ParkInfoLinesR10CUIGrapher   # ParkInfoLines(CUIGrapher&)
  __Z18DoDominantThoughtsv   # DoDominantThoughts()
  __Z18ParkStatusPanelMsgP15InterfaceWindowiii   # ParkStatusPanelMsg(InterfaceWindow*, int, int, int)
  _gParkStatusLastInPark
  _gParkStatusLastTotal
)
