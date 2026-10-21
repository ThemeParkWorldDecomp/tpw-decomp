# Unit v5554: the hire panel's list functions (HirePanelLCPostFormat, HirePanelLCHandler; src/game/ui/ui_hirePanel.cpp). The callees outside the
# unit are recording fakes in tests/unit_HirePanelLCHandler.cpp, which also defines gHirePanelWindow.
set(TPW_UNIT_HirePanelLCHandler src/game/ui/ui_hirePanel.cpp platform/arena.cpp)
set(TPW_UNIT_HirePanelLCHandler_KEEP
  __Z18HirePanelLCHandlerP15InterfaceWindowiii   # HirePanelLCHandler(InterfaceWindow*, int, int, int)
  __Z21HirePanelLCPostFormatP15InterfaceWindow   # HirePanelLCPostFormat(InterfaceWindow*)
  _gHireLCFontIndex
  _gHireListScript
  _gHireListSelA
  _gHireListSelB
)
