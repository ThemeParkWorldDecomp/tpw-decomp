# Unit v5230: InitialiseMainMenu and InitialiseLobbyMenu (src/game/ui/ui_mainmenu.cpp). The callees outside the unit are
# recording fakes in tests/unit_InitialiseLobbyMenu.cpp, which also defines the globals of other files.
set(TPW_UNIT_InitialiseLobbyMenu src/game/ui/ui_mainmenu.cpp platform/arena.cpp)
set(TPW_UNIT_InitialiseLobbyMenu_KEEP
  __Z18InitialiseMainMenuv   # InitialiseMainMenu()
  __Z19InitialiseLobbyMenuv   # InitialiseLobbyMenu()
  TPW_UIMENU_CTOR_EXTERNAL@game/ui/ui_mainmenu.cpp   # the #define TPW_UIMENU_CTOR_EXTERNAL of src/game/ui/ui_mainmenu.cpp (a guard macro; src/game/ui/ui_mainmenu.cpp had it)
)
