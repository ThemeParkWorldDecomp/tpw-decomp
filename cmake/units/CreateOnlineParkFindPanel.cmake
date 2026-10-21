# Unit v5661: CreateOnlineParkFindPanel and ShowFindListSearching (src/game/ui/ui_onlineParkFindPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_CreateOnlineParkFindPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateOnlineParkFindPanel src/game/ui/ui_onlineParkFindPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateOnlineParkFindPanel_KEEP
  __Z21ShowFindListSearchingv   # ShowFindListSearching()
  __Z25CreateOnlineParkFindPanelPFvvE   # CreateOnlineParkFindPanel(void (*)())
  _gOnlineParkFindScript
  _gOnlineParkFindWide
)
set_source_files_properties(src/game/ui/ui_onlineParkFindPanel.cpp PROPERTIES COMPILE_DEFINITIONS TPW_FINDPANEL_UNIT_TEST)
