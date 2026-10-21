# Unit v5565: ShowTagPanel and TagCallback (src/game/ui/ui_tags.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShowTagPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowTagPanel src/game/ui/ui_tags.cpp platform/arena.cpp)
set(TPW_UNIT_ShowTagPanel_KEEP
  __Z11TagCallbackP15InterfaceWindowiii   # TagCallback(InterfaceWindow*, int, int, int)
  __Z12ShowTagPanelP15InterfaceWindow   # ShowTagPanel(InterfaceWindow*)
)
