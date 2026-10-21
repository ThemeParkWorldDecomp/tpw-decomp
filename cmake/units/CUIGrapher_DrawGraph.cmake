# Unit v5160: CUIGrapher::DrawGrid and DrawGraph (src/game/ui/UIGrapher.cpp). The callees outside the unit are
# recording fakes in tests/unit_CUIGrapher_DrawGraph.cpp.
set(TPW_UNIT_CUIGrapher_DrawGraph src/game/ui/UIGrapher.cpp platform/arena.cpp)
set(TPW_UNIT_CUIGrapher_DrawGraph_KEEP
  __ZN10CUIGrapher8DrawGridEv   # CUIGrapher::DrawGrid()
  __ZN10CUIGrapher9DrawGraphEPiii8TbColour   # CUIGrapher::DrawGraph(int*, int, int, TbColour)
  _gGrapherFont
  _gGrapherNumberFormat
)
