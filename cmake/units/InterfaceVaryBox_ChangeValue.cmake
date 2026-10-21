# Unit v5266: InterfaceVaryBox::ChangeValue and SetMinMaxValues (src/game/ui/InterfaceVaryBox.cpp). The callees are recording fakes in
# tests/unit_InterfaceVaryBox_ChangeValue.cpp.
set(TPW_UNIT_InterfaceVaryBox_ChangeValue src/game/ui/InterfaceVaryBox.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceVaryBox_ChangeValue_KEEP
  __ZN16InterfaceVaryBox11ChangeValueEi   # InterfaceVaryBox::ChangeValue(int)
  __ZN16InterfaceVaryBox15SetMinMaxValuesEPiS0_   # InterfaceVaryBox::SetMinMaxValues(int*, int*)
)
