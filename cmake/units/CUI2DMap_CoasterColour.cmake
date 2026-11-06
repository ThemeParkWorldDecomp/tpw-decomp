# Proof unit (candidate 14): the real __sinit_ui_2dmap_cpp, the real UI2DMReturnScaleColour and the real CUI2DMap::DrawCoasterTracks linked together, as the
# shipped game runs them. tests/unit_CUI2DMap_CoasterColour.cpp reuses the fakes of the two units that test them apart.
set(TPW_UNIT_CUI2DMap_CoasterColour src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_CoasterColour_KEEP
  __ZN8CUI2DMap17DrawCoasterTracksEv   # CUI2DMap::DrawCoasterTracks()
  __ZN8CUI2DMap19StoreMapGridDetailsEv   # CUI2DMap::StoreMapGridDetails()
  __ZN8CUI2DMap20UpdateSpritePositionEP14CUI2DMapSprite   # CUI2DMap::UpdateSpritePosition(CUI2DMapSprite*)
  __Z22UI2DMReturnScaleColouriRKN3bfl4draw7CColourES3_S3_   # UI2DMReturnScaleColour
  ___sinit_ui_2dmap_cpp
  _gUI2DMapColours
  _gUI2DMapCounterA
  _gUI2DMapCounterB
  _gUI2DMapCtorVector
  _gUI2DMapDtorVector
  _gUI2DMapGridThingListEnd
  _gUI2DMapImageNodes
  _gUI2DMapImages
  _gUI2DMapNameNode
  _gUI2DMapNameStrings
  _gUI2DMapNode
  _gUI2DMapTgaName
  _gUI2DMapVtableSlot
)
