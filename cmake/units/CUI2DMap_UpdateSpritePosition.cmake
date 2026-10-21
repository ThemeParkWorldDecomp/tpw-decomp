# Unit v5200: CUI2DMap::UpdateSpritePosition, DrawCoasterTracks and StoreMapGridDetails
# (src/game/ui/ui_2dmap.cpp). The callees outside the unit are recording fakes in
# tests/unit_CUI2DMap_UpdateSpritePosition.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_UpdateSpritePosition src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_UpdateSpritePosition_KEEP
  __ZN8CUI2DMap17DrawCoasterTracksEv   # CUI2DMap::DrawCoasterTracks()
  __ZN8CUI2DMap19StoreMapGridDetailsEv   # CUI2DMap::StoreMapGridDetails()
  __ZN8CUI2DMap20UpdateSpritePositionEP14CUI2DMapSprite   # CUI2DMap::UpdateSpritePosition(CUI2DMapSprite*)
  _gUI2DMapColourTable
  _gUI2DMapGridThingListEnd
)
