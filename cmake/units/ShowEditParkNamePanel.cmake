# Unit v5057: the park name panel (ShowEditParkNamePanel, TwoEditsMsg; src/game/ui/ui_entryPricePanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_ShowEditParkNamePanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowEditParkNamePanel src/game/ui/ui_entryPricePanel.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_ShowEditParkNamePanel_KEEP
  __Z11TwoEditsMsgP15InterfaceWindowiii   # TwoEditsMsg(InterfaceWindow*, int, int, int)
  __Z21ShowEditParkNamePanelv   # ShowEditParkNamePanel()
  _gCheatSignA
  _gCheatSignB
  _gEditParkNamePanel
  _gEditParkNameScript
  _gEditSignBufA
  _gEditSignBufB
  _gParkScreenWindow
)
