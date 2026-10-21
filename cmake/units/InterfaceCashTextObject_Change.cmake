# Unit v5983: five text objects' Change and the password destructor (src/game/ui/InterfaceTextObjectSprite.cpp). The callees
# outside the unit are recording fakes in tests/unit_InterfaceCashTextObject_Change.cpp, which also defines gText and the text buffer.
set(TPW_UNIT_InterfaceCashTextObject_Change src/game/ui/InterfaceTextObjectSprite.cpp platform/msl.cpp
    platform/arena.cpp)
set(TPW_UNIT_InterfaceCashTextObject_Change_KEEP
  __Z30InterfaceCashTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfaceCashTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __Z32InterfacePasswordTextObject_DtorP27InterfacePasswordTextObjects   # InterfacePasswordTextObject_Dtor(InterfacePasswordTextObject*, short)
  __Z33InterfacePercentTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfacePercentTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __Z34InterfaceNullCashTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfaceNullCashTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __Z34InterfacePasswordTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfacePasswordTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __Z36InterfaceVisitorNumTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfaceVisitorNumTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __ZN14textobj_callee4LabsEi   # textobj_callee::Labs(int)
  __ZN23InterfaceCashTextObject6ChangeEjP8DOChange   # InterfaceCashTextObject::Change(unsigned int, DOChange*)
  __ZN26InterfacePercentTextObject6ChangeEjP8DOChange   # InterfacePercentTextObject::Change(unsigned int, DOChange*)
  __ZN27InterfaceNullCashTextObject6ChangeEjP8DOChange   # InterfaceNullCashTextObject::Change(unsigned int, DOChange*)
  __ZN29InterfaceVisitorNumTextObject6ChangeEjP8DOChange   # InterfaceVisitorNumTextObject::Change(unsigned int, DOChange*)
)
