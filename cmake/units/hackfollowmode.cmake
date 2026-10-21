# Unit v5076: the staff panel's status and location leaves (src/game/ui/ui_staffPanel.cpp). The callees outside the unit are recording
# fakes in tests/unit_hackfollowmode.cpp, which also defines the globals of other files.
set(TPW_UNIT_hackfollowmode src/game/ui/ui_staffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_hackfollowmode_KEEP
  __Z12UpdateStatusv   # UpdateStatus()
  __Z13UpdateLocatorv   # UpdateLocator()
  __Z14hackfollowmodev   # hackfollowmode()
  __Z19StaffLocationSwitchi   # StaffLocationSwitch(int)
  _gStaffPanelFrame
  _gStaffPanelFrameGuard
  _gStaffPanelLocation
)
