# Unit v5548: the upgrade status panel's window handler and the staff status static initialiser (src/game/ui/ui_staffStatus.cpp, src/game/ui/ui_upgradeStatus.cpp). The callees
# outside the unit are recording fakes in tests/unit_UpgradeStatusMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpgradeStatusMsg src/game/ui/ui_staffStatus.cpp src/game/ui/ui_upgradeStatus.cpp platform/arena.cpp)
set(TPW_UNIT_UpgradeStatusMsg_KEEP
  __Z16UpgradeStatusMsgP15InterfaceWindowiii   # UpgradeStatusMsg(InterfaceWindow*, int, int, int)
  __Z26__sinit_ui_staffStatus_cppv   # __sinit_ui_staffStatus_cpp()
  _gStaffStatusF40
  _gStaffStatusF44
)
