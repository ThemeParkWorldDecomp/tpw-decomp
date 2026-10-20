# Unit v5975: InterfaceTextBox::CalcTextPos and SetText, InterfaceStringHeap::GetString and GetAndCopy, the timer's operator delete
# (src/ui_whatami_InterfaceTextBox_CalcTextPos_2.cpp). The callees outside the unit are recording fakes in
# tests/unit_InterfaceTextBox_CalcTextPos_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceTextBox_CalcTextPos_2 src/ui_whatami_InterfaceTextBox_CalcTextPos_2.cpp platform/arena.cpp)
