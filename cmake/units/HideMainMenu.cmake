# Unit v5303: CreateMainMenu and HideMainMenu (src/game/ui/ui_mainmenu.cpp), with the menu's real destructor (src/game/ui/UIMenu.cpp). The
# other callees are recording fakes in tests/unit_HideMainMenu.cpp.
set(TPW_UNIT_HideMainMenu src/game/ui/ui_mainmenu.cpp src/game/ui/UIMenu.cpp platform/arena.cpp)
set(TPW_UNIT_HideMainMenu_KEEP
  __Z12HideMainMenuv   # HideMainMenu()
  __Z14CreateMainMenui   # CreateMainMenu(int)
  __Z14UIMenuDestructP6UIMenus   # UIMenuDestruct(UIMenu*, short)
  __ZN11menu_callee14OperatorDeleteEPv   # menu_callee::OperatorDelete(void*)
  _gMainMenuPauseFlag
)
