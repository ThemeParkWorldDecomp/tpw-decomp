# Unit v5183: the load, save and hire panels (LoadPanelMsg, UI_HirePanelStateChange, SavePanelMsg; src/LoadPanelMsg.cpp). The callees outside the unit
# are recording fakes in tests/unit_LoadPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_LoadPanelMsg src/LoadPanelMsg.cpp platform/arena.cpp)
