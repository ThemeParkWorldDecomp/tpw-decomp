# Unit v5183: the load, save and hire panels (LoadPanelMsg, UI_HirePanelStateChange, SavePanelMsg; src/game/ui/ui_hirePanel.cpp, src/game/ui/ui_loadSavePanel.cpp). The callees outside the unit
# are recording fakes in tests/unit_LoadPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_LoadPanelMsg src/game/ui/ui_hirePanel.cpp src/game/ui/ui_loadSavePanel.cpp platform/arena.cpp)
set(TPW_UNIT_LoadPanelMsg_KEEP
  __Z12LoadPanelMsgP15InterfaceWindowiii   # LoadPanelMsg(InterfaceWindow*, int, int, int)
  __Z12SavePanelMsgP15InterfaceWindowiii   # SavePanelMsg(InterfaceWindow*, int, int, int)
  __Z23UI_HirePanelStateChangei   # UI_HirePanelStateChange(int)
  _gHirePanelType
  _gHirePanelWindow
  _gLoadPanelOpen
  _gSavePanelName
  _gSavePanelOpen
)
