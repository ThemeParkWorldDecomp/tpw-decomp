# Unit v5698: CreateLoansPanel (src/game/ui/ui_loansPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateLoansPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateLoansPanel src/game/ui/ui_loansPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateLoansPanel_KEEP
  __Z16CreateLoansPanelv   # CreateLoansPanel()
  _gLenderColumnColour
  _gLoansScript
)
