# Unit v5978: the options panel's DoButton and OptionsPanelMsg (src/game/ui/ui_optionsPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_DoButton_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_DoButton_2 src/game/ui/ui_optionsPanel.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_DoButton_2_KEEP
  __Z15OptionsPanelMsgP15InterfaceWindowiii   # OptionsPanelMsg(InterfaceWindow*, int, int, int)
  __ZN13options_panel8DoButtonEii   # options_panel::DoButton(int, int)
  __ZN14options_callee11CallSetTextEP15InterfaceWindowPKDs   # options_callee::CallSetText(InterfaceWindow*, char16_t const*)
  __ZN14options_callee12CallSetValueEP15InterfaceWindowi   # options_callee::CallSetValue(InterfaceWindow*, int)
)
