# Unit v5152: UpdateStaffCost, StaffCostsPanelMsg and ShopStatusMsg (src/game/ui/ui_StaffCostsPanel.cpp, src/game/ui/ui_shopStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_UpdateStaffCost.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateStaffCost src/game/ui/ui_StaffCostsPanel.cpp src/game/ui/ui_shopStatus.cpp platform/arena.cpp)
set(TPW_UNIT_UpdateStaffCost_KEEP
  __Z13ShopStatusMsgP15InterfaceWindowiii   # ShopStatusMsg(InterfaceWindow*, int, int, int)
  __Z15UpdateStaffCostii   # UpdateStaffCost(int, int)
  __Z18StaffCostsPanelMsgP15InterfaceWindowiii   # StaffCostsPanelMsg(InterfaceWindow*, int, int, int)
)
