# Unit v5078: the all-kids panel (FillAllKidsListCallback, UpdateListEntries, AllKidsPanelMsg, LCFormat; src/game/ui/ui_allKidsPanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_AllKidsPanelMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_AllKidsPanelMsg src/game/ui/ui_allKidsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_AllKidsPanelMsg_KEEP
  __Z15AllKidsPanelMsgP15InterfaceWindowiii   # AllKidsPanelMsg(InterfaceWindow*, int, int, int)
  __Z23FillAllKidsListCallbackP6CThing   # FillAllKidsListCallback(CThing*)
  __Z8LCFormatv   # LCFormat()
  __ZN7AllKids17UpdateListEntriesEv   # AllKids::UpdateListEntries()
  _gAllKidsFontIndex
  _gAllKidsListControl
  _gAllKidsListFormat
  _gAllKidsMsgWindow
  _gAllKidsSelectedIndex
)
