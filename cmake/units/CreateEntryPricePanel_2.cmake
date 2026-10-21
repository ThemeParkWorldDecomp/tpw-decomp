# Unit v5885: the entry price panel (CreateEntryPricePanel, EntryPricePanelTitleMsg, EntryPricePanelMsg; src/game/ui/ui_entryPricePanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_CreateEntryPricePanel_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateEntryPricePanel_2 src/game/ui/ui_entryPricePanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateEntryPricePanel_2_KEEP
  __Z18EntryPricePanelMsgP15InterfaceWindowiii   # EntryPricePanelMsg(InterfaceWindow*, int, int, int)
  __Z21CreateEntryPricePanelv   # CreateEntryPricePanel()
  __Z23EntryPricePanelTitleMsgP15InterfaceWindowiii   # EntryPricePanelTitleMsg(InterfaceWindow*, int, int, int)
  __ZN12entry_callee12WindowEnableEP15InterfaceWindows   # entry_callee::WindowEnable(InterfaceWindow*, short)
  _gEntryPriceClosed
  _gEntryPriceFee
  _gEntryPriceScript
)
