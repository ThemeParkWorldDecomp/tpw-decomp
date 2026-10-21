# Unit v5202: SetupGraph, DrawMeGraph and the two GraphLine (src/game/ui/ui_graph.cpp). The callees outside the unit are
# recording fakes in tests/unit_SetupGraph.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetupGraph src/game/ui/ui_graph.cpp platform/arena.cpp)
set(TPW_UNIT_SetupGraph_KEEP
  __Z10SetupGraphR10CUIGrapherR8CoolRectiiiiii   # SetupGraph(CUIGrapher&, CoolRect&, int, int, int, int, int, int)
  __Z11DrawMeGraphP15InterfaceWindowR10CUIGrapherPFvS2_E   # DrawMeGraph(InterfaceWindow*, CUIGrapher&, void (*)(CUIGrapher&))
  __Z9GraphLineR10CUIGrapherP8CHistoryIfLi144EER8TbColourii   # GraphLine(CUIGrapher&, CHistory<float, 144>*, TbColour&, int, int)
  __Z9GraphLineR10CUIGrapherP8CHistoryIiLi144EER8TbColourii   # GraphLine(CUIGrapher&, CHistory<int, 144>*, TbColour&, int, int)
  _gGraphLineBuffer
)
