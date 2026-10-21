# Unit v5279: the online contacts and messages list functions (src/game/ui/ui_onlinePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_ContactsLCFormat.cpp, which also defines the globals of other files.
set(TPW_UNIT_ContactsLCFormat src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_ContactsLCFormat_KEEP
  __Z16ContactsLCFormatv   # ContactsLCFormat()
  __Z17_OnlineAddContactPDsi   # _OnlineAddContact(char16_t*, int)
  __Z20ContactsLCPostFormatP15InterfaceWindow   # ContactsLCPostFormat(InterfaceWindow*)
  __Z20_OnlineRemoveContactPDs   # _OnlineRemoveContact(char16_t*)
  __Z20_OnlineUpdateContactPDsi   # _OnlineUpdateContact(char16_t*, int)
  __Z22ContactsLCEntryHandlerP15InterfaceWindowiii   # ContactsLCEntryHandler(InterfaceWindow*, int, int, int)
  __Z22MessagesLCEntryHandlerP15InterfaceWindowiii   # MessagesLCEntryHandler(InterfaceWindow*, int, int, int)
  _gContactsFontIndex
  _gContactsLCColours
  _gMessagesLCColours
  _gOnlineContactsBusy
)
