# Unit v51004: AllStaffPanelMsg, UIStatusPanel::RequestMaintenance, FinancePanelPanelMsg (src/game/ui/ui_FinancePanel.cpp, src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_allStaffPanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_AllStaffPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_AllStaffPanelMsg src/game/ui/ui_FinancePanel.cpp src/game/ui/ui_StatusPanel.cpp
    src/game/ui/ui_allStaffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_AllStaffPanelMsg_KEEP
  __Z16AllStaffPanelMsgP15InterfaceWindowiii   # AllStaffPanelMsg(InterfaceWindow*, int, int, int)
  __Z20FinancePanelPanelMsgP15InterfaceWindowiii   # FinancePanelPanelMsg(InterfaceWindow*, int, int, int)
  __ZN13UIStatusPanel18RequestMaintenanceEb   # UIStatusPanel::RequestMaintenance(bool)
)
