# Unit v5143: the bumhole window's message function, ToggleGameWindow and SetOnOffText (src/game/ui/ui_mainpanel2.cpp, src/game/ui/ui_optionsPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_bumhole.cpp, which also defines the globals of other files.
set(TPW_UNIT_bumhole src/game/ui/ui_mainpanel2.cpp src/game/ui/ui_optionsPanel.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_bumhole_KEEP
  __Z12SetOnOffTextiibbbi   # SetOnOffText(int, int, bool, bool, bool, int)
  __Z16ToggleGameWindowv   # ToggleGameWindow()
  __Z7bumholeP15InterfaceWindowiii   # bumhole(InterfaceWindow*, int, int, int)
  _gGameWindowToggled
  _gSetOnOffRoot
)
