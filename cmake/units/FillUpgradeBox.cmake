# Unit v5074: the ride status panel's upgrade list (FillUpgradeBox, UpgradePanelMsg; src/game/ui/ui_RideStatus.cpp) and the online park list's LCPostFormat (OnlineParkListPanelLCPostFormat).
# The callees outside the unit are recording fakes in tests/unit_FillUpgradeBox.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillUpgradeBox src/game/ui/ui_RideStatus.cpp platform/arena.cpp)
set(TPW_UNIT_FillUpgradeBox_KEEP
  __Z14FillUpgradeBoxiP21InterfaceListControl2iP7CObject   # FillUpgradeBox(int, InterfaceListControl2*, int, CObject*)
  __Z15UpgradePanelMsgP15InterfaceWindowiii   # UpgradePanelMsg(InterfaceWindow*, int, int, int)
  __Z31OnlineParkListPanelLCPostFormatP15InterfaceWindow   # OnlineParkListPanelLCPostFormat(InterfaceWindow*)
  _gParkListFontIndex
)
