# Unit v5699: BogStatusMsg, __sinit_ui_bogStatus_cpp, CoasterPlaceMouseMode, WhichCheckButtonsAreDown
# (src/game/ui/ui_bogStatus.cpp, src/game/ui/ui_coasterPanel.cpp). The callees outside the unit are recording fakes in tests/unit_BogStatusMsg.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_BogStatusMsg src/game/ui/ui_bogStatus.cpp src/game/ui/ui_coasterPanel.cpp platform/arena.cpp)
set(TPW_UNIT_BogStatusMsg_KEEP
  __Z12BogStatusMsgP15InterfaceWindowiii   # BogStatusMsg(InterfaceWindow*, int, int, int)
  __Z21CoasterPlaceMouseModev   # CoasterPlaceMouseMode()
  __Z24WhichCheckButtonsAreDownj   # WhichCheckButtonsAreDown(unsigned int)
  __Z24__sinit_ui_bogStatus_cppv   # __sinit_ui_bogStatus_cpp()
)
