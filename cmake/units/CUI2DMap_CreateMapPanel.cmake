# Unit v5986: CUI2DMap::CreateMapPanel (src/game/ui/ui_2dmap.cpp).
# The callees outside the unit are recording fakes in tests/unit_CUI2DMap_CreateMapPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_CreateMapPanel src/game/ui/ui_2dmap.cpp platform/arena.cpp
    tests/standins/CUI2DMap_CreateMapPanel.cpp)
set(TPW_UNIT_CUI2DMap_CreateMapPanel_KEEP
  __ZN15mappanel_callee13NewTextSpriteEv   # mappanel_callee::NewTextSprite()
  __ZN8CUI2DMap14CreateMapPanelEv   # CUI2DMap::CreateMapPanel()
  _gMapPanelScript
)
