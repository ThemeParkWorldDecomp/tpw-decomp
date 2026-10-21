# Unit v5066: the all-items panel builder and entry handler, the staff list functions (src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allStaffPanel.cpp). The
# callees outside the unit are recording fakes in tests/unit_CreateAllItemsPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateAllItemsPanel src/game/ui/ui_allItemsPanel.cpp src/game/ui/ui_allStaffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateAllItemsPanel_KEEP
  __Z16FillAllStaffList9StaffTypei   # FillAllStaffList(StaffType, int)
  __Z17UpdateListEntriesv   # UpdateListEntries()
  __Z19CreateAllItemsPanel9UIObjTypeP6CThing   # CreateAllItemsPanel(UIObjType, CThing*)
  __Z22AllItemsLCEntryHandlerP15InterfaceWindowiii   # AllItemsLCEntryHandler(InterfaceWindow*, int, int, int)
  __Z24FillAllStaffListCallbackP6CThing   # FillAllStaffListCallback(CThing*)
  _gAllItemsNoParent
  _gAllItemsScript
  _gStaffListControl
)
