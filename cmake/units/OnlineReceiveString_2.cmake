# Unit v5991: the contacts panel's receive-string, list handler and buddy helpers (src/game/ui/ui_onlinePanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_OnlineReceiveString_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_OnlineReceiveString_2 src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineReceiveString_2_KEEP
  __Z15_OnlineAddBuddyPDsi   # _OnlineAddBuddy(char16_t*, int)
  __Z18ContactListHandlerP15InterfaceWindowiii   # ContactListHandler(InterfaceWindow*, int, int, int)
  __Z18_OnlineRemoveBuddyPDs   # _OnlineRemoveBuddy(char16_t*)
  __Z18_OnlineUpdateBuddyPDsi   # _OnlineUpdateBuddy(char16_t*, int)
  __Z19OnlineReceiveStringPDsi   # OnlineReceiveString(char16_t*, int)
  __ZN15contacts_callee12WindowSlot1cEP15InterfaceWindowi   # contacts_callee::WindowSlot1c(InterfaceWindow*, int)
  __ZN15contacts_callee13FontCharWidthEP13InterfaceFontDs   # contacts_callee::FontCharWidth(InterfaceFont*, char16_t)
  __ZN15contacts_callee16NewOnlineControlEv   # contacts_callee::NewOnlineControl()
  _gContactsMaxRows
)
