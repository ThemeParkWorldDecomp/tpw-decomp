# Unit v5340: UIStatusPanel::ShowEditNamePanel (src/game/ui/ui_StatusPanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_UIStatusPanel_ShowEditNamePanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIStatusPanel_ShowEditNamePanel src/game/ui/ui_StatusPanel.cpp platform/arena.cpp)
set(TPW_UNIT_UIStatusPanel_ShowEditNamePanel_KEEP
  __ZN13UIStatusPanel17ShowEditNamePanelEv   # UIStatusPanel::ShowEditNamePanel()
  _gEditNamePanelScript
)
