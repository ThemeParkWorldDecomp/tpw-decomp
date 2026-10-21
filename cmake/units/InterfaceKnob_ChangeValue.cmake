# Unit v5264: InterfaceKnob::ChangeValue (src/game/ui/InterfaceKnob.cpp). UI_PostMessage and the client draw object
# are recording fakes in tests/unit_InterfaceKnob_ChangeValue.cpp.
set(TPW_UNIT_InterfaceKnob_ChangeValue src/game/ui/InterfaceKnob.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceKnob_ChangeValue_KEEP
  __ZN13InterfaceKnob11ChangeValueEi   # InterfaceKnob::ChangeValue(int)
)
