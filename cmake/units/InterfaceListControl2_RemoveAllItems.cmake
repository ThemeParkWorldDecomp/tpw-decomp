# Unit v5095: InterfaceListControl2 RemoveAllItems, RemoveItem, RemoveMe (src/game/ui/InterfaceListControl2.cpp). The callees
# outside the unit are recording fakes in tests/unit_InterfaceListControl2_RemoveAllItems.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceListControl2_RemoveAllItems src/game/ui/InterfaceListControl2.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceListControl2_RemoveAllItems_KEEP
  __ZN21InterfaceListControl210RemoveItemEi   # InterfaceListControl2::RemoveItem(int)
  __ZN21InterfaceListControl214RemoveAllItemsEv   # InterfaceListControl2::RemoveAllItems()
  __ZN21InterfaceListControl28RemoveMeEiiii   # InterfaceListControl2::RemoveMe(int, int, int, int)
)
