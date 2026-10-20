# Unit v5074: the ride status panel's upgrade list (FillUpgradeBox, UpgradePanelMsg; src/FillUpgradeBox.cpp) and the online park list's LCPostFormat (OnlineParkListPanelLCPostFormat).
# The callees outside the unit are recording fakes in tests/unit_FillUpgradeBox.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillUpgradeBox src/FillUpgradeBox.cpp platform/arena.cpp)
