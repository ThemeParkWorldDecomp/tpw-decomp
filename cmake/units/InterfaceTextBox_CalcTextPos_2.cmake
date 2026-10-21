# Unit v5975: InterfaceTextBox::CalcTextPos and SetText, InterfaceStringHeap::GetString and GetAndCopy, the timer's operator delete
# (src/game/ui/InterfaceStringHeap.cpp, src/game/ui/InterfaceTextBox.cpp). The callees outside the unit are recording fakes in
# tests/unit_InterfaceTextBox_CalcTextPos_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceTextBox_CalcTextPos_2 src/game/ui/InterfaceStringHeap.cpp src/game/ui/InterfaceTextBox.cpp
    platform/arena.cpp)
set(TPW_UNIT_InterfaceTextBox_CalcTextPos_2_KEEP
  __ZN14InterfaceTimer14OperatorDeleteEPv   # InterfaceTimer::OperatorDelete(void*)
  __ZN14textbox_callee6DeleteEPv   # textbox_callee::Delete(void*)
  __ZN14textbox_callee8NewArrayEj   # textbox_callee::NewArray(unsigned int)
  __ZN16InterfaceTextBox11CalcTextPosEv   # InterfaceTextBox::CalcTextPos()
  __ZN16InterfaceTextBox7SetTextEPDs   # InterfaceTextBox::SetText(char16_t*)
  __ZN19InterfaceStringHeap10GetAndCopyEPKDs   # InterfaceStringHeap::GetAndCopy(char16_t const*)
  __ZN19InterfaceStringHeap9GetStringEi   # InterfaceStringHeap::GetString(int)
)
