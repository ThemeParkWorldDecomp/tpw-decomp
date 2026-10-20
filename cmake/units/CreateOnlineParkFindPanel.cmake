# Unit v5661: CreateOnlineParkFindPanel and ShowFindListSearching (src/OnlineParkInfoPanelMsg_CreateOnlineParkFindPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_CreateOnlineParkFindPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateOnlineParkFindPanel src/OnlineParkInfoPanelMsg_CreateOnlineParkFindPanel.cpp platform/arena.cpp)
set_source_files_properties(src/OnlineParkInfoPanelMsg_CreateOnlineParkFindPanel.cpp PROPERTIES COMPILE_DEFINITIONS TPW_FINDPANEL_UNIT_TEST)
