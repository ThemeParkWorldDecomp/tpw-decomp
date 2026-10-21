# Unit v5335: UpdateSideshowListEntries and the all-items row window (LCFormat 0x10196388); src/game/ui/ui_allItemsPanel.cpp.
# The callees outside the unit are recording fakes in tests/unit_UpdateSideshowListEntries.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateSideshowListEntries src/game/ui/ui_allItemsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_UpdateSideshowListEntries_KEEP
  __Z20AllItemsListLCFormatv   # AllItemsListLCFormat()
  __Z25UpdateSideshowListEntriesP21InterfaceListControl2   # UpdateSideshowListEntries(InterfaceListControl2*)
  __ZN15allitems_callee11FontVirtualEP13InterfaceFont   # allitems_callee::FontVirtual(InterfaceFont*)
  _gAllItemsFontIndex
  _gAllItemsRowHeight
)
