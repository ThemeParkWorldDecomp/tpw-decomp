# Unit v5936: ShopStatusPanel::InitStatusWindow and UpdateWindow (src/game/ui/ui_shopStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_ShopStatusPanel_InitStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShopStatusPanel_InitStatusWindow src/game/ui/ui_shopStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShopStatusPanel_InitStatusWindow_KEEP
  __ZN15ShopStatusPanel12UpdateWindowEv   # ShopStatusPanel::UpdateWindow()
  __ZN15ShopStatusPanel16InitStatusWindowEv   # ShopStatusPanel::InitStatusWindow()
  _gShopStatusMeshNames
)
