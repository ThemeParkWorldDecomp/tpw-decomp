# Unit v5262: LinkSlideoutPanel, RemoveSlideoutPanel (src/game/ui/ui_mainpanel2.cpp). The callees outside the unit are
# recording fakes in tests/unit_LinkSlideoutPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_LinkSlideoutPanel src/game/ui/ui_mainpanel2.cpp platform/arena.cpp)
set(TPW_UNIT_LinkSlideoutPanel_KEEP
  __Z17LinkSlideoutPanelP15InterfaceWindowi   # LinkSlideoutPanel(InterfaceWindow*, int)
  __Z19RemoveSlideoutPanels   # RemoveSlideoutPanel(short)
  __Z23LinkSlideoutPanelResultP15InterfaceWindowi   # LinkSlideoutPanelResult(InterfaceWindow*, int)
)
