# Unit v5021: CreateResearchPanel (src/game/ui/ui_researchPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateResearchPanel.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CreateResearchPanel src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateResearchPanel_KEEP
  __Z19CreateResearchPanelv   # CreateResearchPanel()
  _gResearchCurrentAbility
  _gResearchEffortBar
  _gResearchFontIndex
  _gResearchGaugeValue
  _gResearchMaxAbility
  _gResearchPanel
  _gResearchPanelMade
  _gResearchPanelScript
  _gResearchSoundA
  _gResearchSoundB
)
