# Unit v5908: InterfaceListControl2 FindData, PropagateMessage, SelectClick, SetCurrent, SetSort, ColumnButtonClick
# (src/game/ui/InterfaceListControl2.cpp). The callees outside the unit are recording fakes in
# tests/unit_InterfaceListControl2_SetSort.cpp.
set(TPW_UNIT_InterfaceListControl2_SetSort src/game/ui/InterfaceListControl2.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceListControl2_SetSort_KEEP
  __Z14WindowSetValueP15InterfaceWindowi   # WindowSetValue(InterfaceWindow*, int)
  __ZN21InterfaceListControl210SetCurrentEi   # InterfaceListControl2::SetCurrent(int)
  __ZN21InterfaceListControl211SelectClickEss   # InterfaceListControl2::SelectClick(short, short)
  __ZN21InterfaceListControl216PropagateMessageEiii   # InterfaceListControl2::PropagateMessage(int, int, int)
  __ZN21InterfaceListControl217ColumnButtonClickEi   # InterfaceListControl2::ColumnButtonClick(int)
  __ZN21InterfaceListControl27SetSortEib   # InterfaceListControl2::SetSort(int, bool)
  __ZN21InterfaceListControl28FindDataEi   # InterfaceListControl2::FindData(int)
)
