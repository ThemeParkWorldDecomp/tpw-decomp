# Unit v5712: InterfaceTextBox::operator- (src/game/ui/InterfaceTextBox.cpp). The callees outside the unit are recording
# fakes in tests/unit_InterfaceTextBox_operator.cpp.
set(TPW_UNIT_InterfaceTextBox_operator src/game/ui/InterfaceTextBox.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceTextBox_operator_KEEP
  __ZN16InterfaceTextBoxmiER15InterfaceWindow   # InterfaceTextBox::operator-(InterfaceWindow&)
)
