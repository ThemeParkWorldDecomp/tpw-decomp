# Unit v5195: the loans panel (LoansPanelMsg, Payback, UpdateListHeadings; src/game/ui/ui_loansPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_LoansPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_LoansPanelMsg src/game/ui/ui_loansPanel.cpp platform/arena.cpp)
set(TPW_UNIT_LoansPanelMsg_KEEP
  __Z13LoansPanelMsgP15InterfaceWindowiii   # LoansPanelMsg(InterfaceWindow*, int, int, int)
  __Z18UpdateListHeadingsv   # UpdateListHeadings()
  __Z7Paybacki   # Payback(int)
  _gLoansPanel
)
