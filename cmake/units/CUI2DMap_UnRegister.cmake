# Unit v5959: the 2D map's sprite list and map picture functions (UnRegister, DeleteSpriteListEntries, UpdateAllSpritePositions,
# CloseSprites, Zoom, LoadMap, UpdateMap; src/game/ui/ui_2dmap.cpp).
# The callees outside the unit are recording fakes in tests/unit_CUI2DMap_UnRegister.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_UnRegister src/game/ui/ui_2dmap.cpp platform/arena.cpp tests/standins/CUI2DMap_UnRegister.cpp)
set(TPW_UNIT_CUI2DMap_UnRegister_KEEP
  __ZN8CUI2DMap10UnRegisterEP14CUI2DMapSprite   # CUI2DMap::UnRegister(CUI2DMapSprite*)
  __ZN8CUI2DMap12CloseSpritesEv   # CUI2DMap::CloseSprites()
  __ZN8CUI2DMap23DeleteSpriteListEntriesEv   # CUI2DMap::DeleteSpriteListEntries()
  __ZN8CUI2DMap24UpdateAllSpritePositionsEv   # CUI2DMap::UpdateAllSpritePositions()
  __ZN8CUI2DMap4ZoomE12EUI2DMapZoomf   # CUI2DMap::Zoom(EUI2DMapZoom, float)
  __ZN8CUI2DMap7LoadMapEi   # CUI2DMap::LoadMap(int)
  __ZN8CUI2DMap9UpdateMapEv   # CUI2DMap::UpdateMap()
  _gUI2DMapSeparator
)
