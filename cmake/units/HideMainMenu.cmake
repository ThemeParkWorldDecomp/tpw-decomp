# Unit v5303: CreateMainMenu and HideMainMenu (src/game/ui/ui_mainmenu.cpp). The callees are recording fakes in tests/unit_HideMainMenu.cpp.
set(TPW_UNIT_HideMainMenu src/game/ui/ui_mainmenu.cpp platform/arena.cpp)
set(TPW_UNIT_HideMainMenu_KEEP
  __Z12HideMainMenuv   # HideMainMenu()
  __Z13DestroyUIMenuP6UIMenu   # DestroyUIMenu(UIMenu*)
  __Z14CreateMainMenui   # CreateMainMenu(int)
  _gMainMenuPauseFlag
)
