# Unit v5634: CreateOptionsPanel 0x101a9fb8 (src/game/ui/ui_optionsPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateOptionsPanel.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CreateOptionsPanel src/game/ui/ui_optionsPanel.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_CreateOptionsPanel_KEEP
  __Z18CreateOptionsPanelv   # CreateOptionsPanel()
  _gOptionsPanelF0C
  _gOptionsPanelFontIndex
  _gOptionsPanelGameRunning
  _gOptionsPanelRenderOptions
)
