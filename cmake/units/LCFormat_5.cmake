# Unit v5702: the save panel's row maker and OK handler (src/game/ui/ui_loadSavePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_LCFormat_5.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCFormat_5 src/game/ui/ui_loadSavePanel.cpp platform/arena.cpp)
set(TPW_UNIT_LCFormat_5_KEEP
  __Z15SaveCallback_OKPv   # SaveCallback_OK(void*)
  __Z17SavePanelLCFormatv   # SavePanelLCFormat()
  _gSavePanelListFormat
)
