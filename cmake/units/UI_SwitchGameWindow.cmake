# Unit v5313: UI_SwitchGameWindow, UI_SetGameWindowCursor (src/game/ui/ui_mainpanel2.cpp). The callees are
# recording fakes in tests/unit_UI_SwitchGameWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_UI_SwitchGameWindow src/game/ui/ui_mainpanel2.cpp platform/arena.cpp)
set(TPW_UNIT_UI_SwitchGameWindow_KEEP
  __Z19UI_SwitchGameWindowi   # UI_SwitchGameWindow(int)
  __Z22UI_SetGameWindowCursors   # UI_SetGameWindowCursor(short)
  _gGameWindowCursor
)
