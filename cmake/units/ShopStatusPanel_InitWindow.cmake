# Unit v5085: the shop status panel (ShopStatusPanel::InitWindow, UpdateStatusWindow; src/game/ui/ui_shopStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShopStatusPanel_InitWindow.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_ShopStatusPanel_InitWindow src/game/ui/ui_shopStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShopStatusPanel_InitWindow_KEEP
  __ZN11shop_callee11OperatorNewEj   # shop_callee::OperatorNew(unsigned int)
  __ZN15ShopStatusPanel10InitWindowEv   # ShopStatusPanel::InitWindow()
  __ZN15ShopStatusPanel18UpdateStatusWindowEv   # ShopStatusPanel::UpdateStatusWindow()
  _gShopStatusObject
)
