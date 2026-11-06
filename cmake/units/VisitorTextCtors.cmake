# Bug class of the translation scans: the visitor time and visitor number text objects the status panel and the all-kids list construct, with the
# real glue, the real classes and the real InterfaceWindow::SetValue (tests/test_VisitorTextCtors.py, tests/unit_VisitorTextCtors.cpp).
set(TPW_UNIT_VisitorTextCtors src/game/ui/InterfaceTextObjectSprite.cpp src/game/ui/InterfaceWindow.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_VisitorTextCtors_KEEP
  __ZN10vsp_callee8TimeCtorEPv   # vsp_callee::TimeCtor(void*)
  __ZN14kidlist_callee14VisitorNumCtorEPv   # kidlist_callee::VisitorNumCtor(void*)
  __ZN15InterfaceWindow8SetValueEi   # InterfaceWindow::SetValue(int)
  __ZN25InterfaceTextObjectSpriteC1EPv   # InterfaceTextObjectSprite::InterfaceTextObjectSprite(void*)
  __ZN29InterfaceVisitorNumTextObjectC1EPv   # InterfaceVisitorNumTextObject::InterfaceVisitorNumTextObject(void*)
  __ZN29InterfaceVisitorNumTextObjectD1Ev   # InterfaceVisitorNumTextObject::~InterfaceVisitorNumTextObject()
  __ZN29InterfaceVisitorNumTextObject6ChangeEjP8DOChange   # InterfaceVisitorNumTextObject::Change(unsigned int, DOChange*)
  __Z36InterfaceVisitorNumTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfaceVisitorNumTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __ZN30InterfaceVisitorTimeTextObjectC1EPv   # InterfaceVisitorTimeTextObject::InterfaceVisitorTimeTextObject(void*)
  __ZN30InterfaceVisitorTimeTextObjectD1Ev   # InterfaceVisitorTimeTextObject::~InterfaceVisitorTimeTextObject()
  __ZN30InterfaceVisitorTimeTextObject6ChangeEjP8DOChange   # InterfaceVisitorTimeTextObject::Change(unsigned int, DOChange*)
)
