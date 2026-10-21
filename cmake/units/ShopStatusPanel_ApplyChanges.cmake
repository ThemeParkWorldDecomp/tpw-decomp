# Unit v5734: ShopStatusPanel::ApplyChanges, applytoallcallback and Show (src/game/ui/ui_shopStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShopStatusPanel_ApplyChanges.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShopStatusPanel_ApplyChanges src/game/ui/ui_shopStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShopStatusPanel_ApplyChanges_KEEP
  __ZN15ShopStatusPanel12ApplyChangesEv   # ShopStatusPanel::ApplyChanges()
  __ZN15ShopStatusPanel18applytoallcallbackEP6CThing   # ShopStatusPanel::applytoallcallback(CThing*)
  __ZN15ShopStatusPanel4ShowEP6CThing   # ShopStatusPanel::Show(CThing*)
  _gShopStatusDescription
)
