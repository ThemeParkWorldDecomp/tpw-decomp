# Unit v5343: ShowStaffPanel and ShowKidLocatorPanel (src/game/ui/ui_staffPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShowKidLocatorPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowKidLocatorPanel src/game/ui/ui_staffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_ShowKidLocatorPanel_KEEP
  __Z14ShowStaffPanelP6CThing   # ShowStaffPanel(CThing*)
  __Z19ShowKidLocatorPanelv   # ShowKidLocatorPanel()
)
