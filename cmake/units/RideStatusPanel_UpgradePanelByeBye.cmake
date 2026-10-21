# Unit v51121: the small functions of the ride and shop status panels, the staff costs and research panel removers, the static initialisers
# and StoreTankMsg (src/game/ui/ui_RideStatus.cpp, src/game/ui/ui_StaffCostsPanel.cpp, src/game/ui/ui_researchPanel.cpp, src/game/ui/ui_ridepanel.cpp, src/game/ui/ui_shopStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_RideStatusPanel_UpgradePanelByeBye.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideStatusPanel_UpgradePanelByeBye src/game/ui/ui_RideStatus.cpp src/game/ui/ui_StaffCostsPanel.cpp
    src/game/ui/ui_researchPanel.cpp src/game/ui/ui_ridepanel.cpp src/game/ui/ui_shopStatus.cpp platform/arena.cpp)
set(TPW_UNIT_RideStatusPanel_UpgradePanelByeBye_KEEP
  __Z12StoreTankMsgP15InterfaceWindowiii   # StoreTankMsg(InterfaceWindow*, int, int, int)
  __Z16_applytoall_shopP6CThing   # _applytoall_shop(CThing*)
  __Z19RemoveResearchPanelv   # RemoveResearchPanel()
  __Z21RemoveStaffCostsPanelv   # RemoveStaffCostsPanel()
  __Z24__sinit_ui_ridepanel_cppv   # __sinit_ui_ridepanel_cpp()
  __Z30__sinit_ui_StaffCostsPanel_cppv   # __sinit_ui_StaffCostsPanel_cpp()
  __ZN15RideStatusPanel12DeInitWindowEv   # RideStatusPanel::DeInitWindow()
  __ZN15RideStatusPanel14InitViewWindowEv   # RideStatusPanel::InitViewWindow()
  __ZN15RideStatusPanel16DeInitViewWindowEv   # RideStatusPanel::DeInitViewWindow()
  __ZN15RideStatusPanel16UpdateViewWindowEv   # RideStatusPanel::UpdateViewWindow()
  __ZN15RideStatusPanel18DeInitStatusWindowEv   # RideStatusPanel::DeInitStatusWindow()
  __ZN15RideStatusPanel18UpgradePanelByeByeEv   # RideStatusPanel::UpgradePanelByeBye()
  __ZN15RideStatusPanel6RemoveEv   # RideStatusPanel::Remove()
  __ZN15RideStatusPanel9BreakDownE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # RideStatusPanel::BreakDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN15ShopStatusPanel12DeInitWindowEv   # ShopStatusPanel::DeInitWindow()
  __ZN15ShopStatusPanel14InitViewWindowEv   # ShopStatusPanel::InitViewWindow()
  __ZN15ShopStatusPanel16DeInitViewWindowEv   # ShopStatusPanel::DeInitViewWindow()
  __ZN15ShopStatusPanel16UpdateViewWindowEv   # ShopStatusPanel::UpdateViewWindow()
  __ZN15ShopStatusPanel18DeInitStatusWindowEv   # ShopStatusPanel::DeInitStatusWindow()
  __ZN15ShopStatusPanel6RemoveEv   # ShopStatusPanel::Remove()
  _gStaffCostsF24
  _gStaffCostsF28
)
