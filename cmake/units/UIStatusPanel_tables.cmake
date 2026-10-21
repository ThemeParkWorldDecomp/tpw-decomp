# Unit v5L31: the status panels' tables of virtual slots (src/game/ui/ui_StatusPanel.cpp). The members they name are recording fakes in
# tests/unit_UIStatusPanel_tables.cpp.
set(TPW_UNIT_UIStatusPanel_tables src/game/ui/ui_StatusPanel.cpp platform/arena.cpp)
set(TPW_UNIT_UIStatusPanel_tables_KEEP
  __Z18UIStatusPanelTablev   # UIStatusPanelTable()
  __Z19BogStatusPanelTablev   # BogStatusPanelTable()
  __Z20RideStatusPanelTablev   # RideStatusPanelTable()
  __Z20ShopStatusPanelTablev   # ShopStatusPanelTable()
  __Z20ShowStatusPanelTablev   # ShowStatusPanelTable()
  __Z21StaffStatusPanelTablev   # StaffStatusPanelTable()
  __Z23FeatureStatusPanelTablev   # FeatureStatusPanelTable()
  __Z23UpgradeStatusPanelTablev   # UpgradeStatusPanelTable()
  __Z23VisitorStatusPanelTablev   # VisitorStatusPanelTable()
  __Z25StaffRoomStatusPanelTablev   # StaffRoomStatusPanelTable()
)
