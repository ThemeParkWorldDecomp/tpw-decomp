# Unit v5913: CreateYearEndPanel (src/game/ui/ui_yearEndPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_CreateYearEndPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateYearEndPanel src/game/ui/ui_yearEndPanel.cpp platform/arena.cpp
    tests/standins/CreateYearEndPanel.cpp)
set(TPW_UNIT_CreateYearEndPanel_KEEP
  __Z18CreateYearEndPanelv   # CreateYearEndPanel()
  _gYearEndPanel
  _gYearEndScript
)
