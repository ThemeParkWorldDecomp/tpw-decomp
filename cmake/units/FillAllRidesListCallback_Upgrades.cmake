# Unit v5952: the rides upgrades, shops and features list rows, AllKidsPanel_KidGone and the kids list's LCPostFormat
# (src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_FillAllRidesListCallback_Upgrades.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillAllRidesListCallback_Upgrades src/game/ui/ui_allItemsPanel.cpp src/game/ui/ui_allKidsPanel.cpp
    platform/arena.cpp)
set(TPW_UNIT_FillAllRidesListCallback_Upgrades_KEEP
  __Z20AllKidsPanel_KidGoneP6CThing   # AllKidsPanel_KidGone(CThing*)
  __Z23AllKidsListLCPostFormatP15InterfaceWindow   # AllKidsListLCPostFormat(InterfaceWindow*)
  __Z24FillAllShopsListCallbackP6CThing   # FillAllShopsListCallback(CThing*)
  __Z27FillAllFeaturesListCallbackP6CThing   # FillAllFeaturesListCallback(CThing*)
  __Z33FillAllRidesListCallback_UpgradesP6CThing   # FillAllRidesListCallback_Upgrades(CThing*)
  __ZN14kidlist_callee14VisitorNumCtorEPv   # kidlist_callee::VisitorNumCtor(void*)
  __ZN14kidlist_callee8RectCtorEPv   # kidlist_callee::RectCtor(void*)
  __ZN14kidlist_callee8RectDtorEPv   # kidlist_callee::RectDtor(void*)
  _gAllRidesNoUpgradeParent
)
