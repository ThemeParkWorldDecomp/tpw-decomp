# Unit v5073: the finance graph panel message handler (FinanceGraphPanelMsg; src/game/ui/ui_FinancePanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_FinanceGraphPanelMsg.cpp.
set(TPW_UNIT_FinanceGraphPanelMsg src/game/ui/ui_FinancePanel.cpp platform/arena.cpp)
set(TPW_UNIT_FinanceGraphPanelMsg_KEEP
  __Z20FinanceGraphPanelMsgP15InterfaceWindowiii   # FinanceGraphPanelMsg(InterfaceWindow*, int, int, int)
  _gFinanceGraphFlags
  _gFinanceGraphMode
  _gFinanceGraphPanel
  _gFinanceGrapher
)
