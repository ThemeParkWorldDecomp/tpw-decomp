# Unit v5002: the all-items panel's message functions and the all-features panel (src/game/ui/ui_allItemsPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_MainWindowCallback.cpp.
set(TPW_UNIT_MainWindowCallback src/game/ui/ui_allItemsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_MainWindowCallback_KEEP
  __Z18ActualListCallbackP15InterfaceWindowiii   # ActualListCallback(InterfaceWindow*, int, int, int)
  __Z18MainWindowCallbackP15InterfaceWindowiii   # MainWindowCallback(InterfaceWindow*, int, int, int)
  __Z20ListTemplateCallbackP15InterfaceWindowiii   # ListTemplateCallback(InterfaceWindow*, int, int, int)
  __Z22CreateAllFeaturesPanelP6CThing   # CreateAllFeaturesPanel(CThing*)
  _gAllColumnButtonFont
  _gAllFeaturesScript
  _gAllFeaturesSort
  _gAllItemsList
  _gAllItemsListType
  _gAllItemsPanel
  _gAllRidesSort
  _gAllShopsSort
  _gAllShowsSort
)
