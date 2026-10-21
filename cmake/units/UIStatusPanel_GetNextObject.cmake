# Unit v5090: UIStatusPanel GetNextObject, FillObjectList, EraseObject, OpenWindow, Move (src/game/ui/ui_StatusPanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_UIStatusPanel_GetNextObject.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIStatusPanel_GetNextObject src/game/ui/ui_StatusPanel.cpp platform/arena.cpp)
set(TPW_UNIT_UIStatusPanel_GetNextObject_KEEP
  __ZN13UIStatusPanel10OpenWindowEPsPFiP15InterfaceWindowiiiEiii   # UIStatusPanel::OpenWindow(short*, int (*)(InterfaceWindow*, int, int, int), int, int, int)
  __ZN13UIStatusPanel11EraseObjectEv   # UIStatusPanel::EraseObject()
  __ZN13UIStatusPanel13GetNextObjectEv   # UIStatusPanel::GetNextObject()
  __ZN13UIStatusPanel14FillObjectListEP6CThing   # UIStatusPanel::FillObjectList(CThing*)
  __ZN13UIStatusPanel4MoveEv   # UIStatusPanel::Move()
  _gStatusPanelClickStackDir
  _gStatusPanelListCount
  _gStatusPanelMoveObject
)
