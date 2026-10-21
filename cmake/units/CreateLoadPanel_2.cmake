# Unit v5969: CreateLoadPanel, CreateSavePanel, FillLoadList, FillSaveList and the list attach function LCPostFormat
# (src/game/ui/ui_loadSavePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateLoadPanel_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateLoadPanel_2 src/game/ui/ui_loadSavePanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateLoadPanel_2_KEEP
  __Z12FillLoadListP21InterfaceListControl2   # FillLoadList(InterfaceListControl2*)
  __Z12FillSaveListP21InterfaceListControl2   # FillSaveList(InterfaceListControl2*)
  __Z15CreateLoadPaneli   # CreateLoadPanel(int)
  __Z15CreateSavePaneli   # CreateSavePanel(int)
  __Z20LoadSaveLCPostFormatP15InterfaceWindow   # LoadSaveLCPostFormat(InterfaceWindow*)
  __ZN15loadsave_callee13CursorPosCtorEPv   # loadsave_callee::CursorPosCtor(void*)
  __ZN15loadsave_callee15FileTimeToLocalEPKvPv   # loadsave_callee::FileTimeToLocal(void const*, void*)
  _gLoadSaveFormat
  _gLoadSaveHilight
  _gLoadSaveScript
)
