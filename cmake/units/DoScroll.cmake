# Unit v5097: the research panel's DoScroll, InitAll and ResearchPanelMsg (src/game/ui/ui_researchPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_DoScroll.cpp, which also defines the globals of other files.
set(TPW_UNIT_DoScroll src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_DoScroll_KEEP
  __Z16ResearchPanelMsgP15InterfaceWindowiii   # ResearchPanelMsg(InterfaceWindow*, int, int, int)
  __Z7InitAllv   # InitAll()
  __Z8DoScrollii   # DoScroll(int, int)
  _gResearchScrollSound
)
