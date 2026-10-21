# Unit v5955: the list control's GetColumnPos, GetItem, SetData, GetNonHierIndexedEntry and UI_DefaultListControl2Callback
# (src/game/ui/InterfaceListControl2.cpp). The callees outside the unit are recording fakes in
# tests/unit_InterfaceListControl2_GetColumnPos.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceListControl2_GetColumnPos src/game/ui/InterfaceListControl2.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceListControl2_GetColumnPos_KEEP
  __Z30UI_DefaultListControl2CallbackP15InterfaceWindowiii   # UI_DefaultListControl2Callback(InterfaceWindow*, int, int, int)
  __ZN21InterfaceListControl212GetColumnPosEiPsS0_   # InterfaceListControl2::GetColumnPos(int, short*, short*)
  __ZN21InterfaceListControl222GetNonHierIndexedEntryEii   # InterfaceListControl2::GetNonHierIndexedEntry(int, int)
  __ZN21InterfaceListControl27GetItemEiiR6LCWord   # InterfaceListControl2::GetItem(int, int, LCWord&)
  __ZN21InterfaceListControl27SetDataEii   # InterfaceListControl2::SetData(int, int)
  _gListControl2Keys
)
