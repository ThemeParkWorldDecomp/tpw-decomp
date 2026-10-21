# Unit v5964: the all-rides and all-shops panels (src/game/ui/ui_allItemsPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_CreateAllRidesPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateAllRidesPanel src/game/ui/ui_allItemsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateAllRidesPanel_KEEP
  __Z19CreateAllRidesPanelP6CThing   # CreateAllRidesPanel(CThing*)
  __Z19CreateAllShopsPanelP6CThing   # CreateAllShopsPanel(CThing*)
  _gAllRidesScript
)
