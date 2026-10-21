# Unit v5203: InterfaceListControl2::Sort and FindString (src/game/ui/InterfaceListControl2.cpp). The callees outside the unit
# are recording fakes in tests/unit_InterfaceListControl2_Sort.cpp.
set(TPW_UNIT_InterfaceListControl2_Sort src/game/ui/InterfaceListControl2.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceListControl2_Sort_KEEP
  __ZN21InterfaceListControl210FindStringEPDsi   # InterfaceListControl2::FindString(char16_t*, int)
  __ZN21InterfaceListControl24SortEi   # InterfaceListControl2::Sort(int)
)
