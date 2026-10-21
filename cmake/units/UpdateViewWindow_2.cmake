# Unit v5984: the hire panel's functions (UI_FinancePanelPanelDateChange, FillHireList, UpdateViewWindow, HirePanelMsg, LCFormat;
# src/game/ui/ui_FinancePanel.cpp, src/game/ui/ui_hirePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_UpdateViewWindow_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateViewWindow_2 src/game/ui/ui_FinancePanel.cpp src/game/ui/ui_hirePanel.cpp platform/arena.cpp
    tests/standins/UpdateViewWindow_2.cpp)
set(TPW_UNIT_UpdateViewWindow_2_KEEP
  __Z12FillHireList9StaffTypei   # FillHireList(StaffType, int)
  __Z12HirePanelMsgP15InterfaceWindowiii   # HirePanelMsg(InterfaceWindow*, int, int, int)
  __Z16HireListLCFormatv   # HireListLCFormat()
  __Z16UpdateViewWindowi   # UpdateViewWindow(int)
  __Z30UI_FinancePanelPanelDateChangei   # UI_FinancePanelPanelDateChange(int)
  _gHireListFormat
)
