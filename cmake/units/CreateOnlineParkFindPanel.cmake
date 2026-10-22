# Unit v5661: CreateOnlineParkFindPanel and ShowFindListSearching (src/game/ui/ui_onlineParkFindPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_CreateOnlineParkFindPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateOnlineParkFindPanel src/game/ui/ui_onlineParkFindPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateOnlineParkFindPanel_KEEP
  __Z21ShowFindListSearchingv   # ShowFindListSearching()
  __Z25CreateOnlineParkFindPanelPFvvE   # CreateOnlineParkFindPanel(void (*)())
  _gOnlineParkFindScript
  _gOnlineParkFindWide
)
tpw_test_source_flags(src/game/ui/ui_onlineParkFindPanel.cpp DEFINITIONS TPW_FINDPANEL_UNIT_TEST)
