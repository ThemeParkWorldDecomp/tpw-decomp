# Unit v5097: the research panel's DoScroll, InitAll and ResearchPanelMsg (src/game/ui/ui_researchPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_DoScroll.cpp, which also defines the globals of other files.
set(TPW_UNIT_DoScroll src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_DoScroll_KEEP
  __Z16ResearchPanelMsgP15InterfaceWindowiii   # ResearchPanelMsg(InterfaceWindow*, int, int, int)
  __Z7InitAllv   # InitAll()
  __Z8DoScrollii   # DoScroll(int, int)
  _gResearchScrollSound
)
# The recording fake of UIMain_UISound in tests/unit_DoScroll.cpp is spelled with the length prefix 13 (ui_researchPanel.cpp keeps that label in the test copies).
tpw_test_source_flags(src/game/ui/ui_researchPanel.cpp DEFINITIONS TPW_DOSCROLL_FAKES)
