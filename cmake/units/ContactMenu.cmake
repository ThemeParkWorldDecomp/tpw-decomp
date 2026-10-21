# Unit v5660: the online panel's contact helpers (FillBuddyList, ButtonsMenuMsg, ContactMenu; src/game/ui/ui_onlinePanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_ContactMenu.cpp, which also defines the globals of other files.
set(TPW_UNIT_ContactMenu src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_ContactMenu_KEEP
  __Z11ContactMenui   # ContactMenu(int)
  __Z13FillBuddyListv   # FillBuddyList()
  __Z14ButtonsMenuMsgP15InterfaceWindowiii   # ButtonsMenuMsg(InterfaceWindow*, int, int, int)
)
