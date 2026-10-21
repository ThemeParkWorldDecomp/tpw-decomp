# Unit v5295: UI_ToggleHelpText and __sinit_UI_Base_cpp (src/game/ui/UI_Base.cpp). The callees outside the unit are recording
# fakes in tests/unit_sinit_UI_Base_cpp.cpp, which also defines the globals of other files.
set(TPW_UNIT_sinit_UI_Base_cpp src/game/ui/UI_Base.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_UI_Base_cpp_KEEP
  __Z17UI_ToggleHelpTextv   # UI_ToggleHelpText()
  __Z19__sinit_UI_Base_cppv   # __sinit_UI_Base_cpp()
  __Z25UI_DestroyDefaultHelpTextP17InterfaceHelpTexts   # UI_DestroyDefaultHelpText(InterfaceHelpText*, short)
  _gUIDefaultHelpTextNode
)
