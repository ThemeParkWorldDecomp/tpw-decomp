# Unit v5083: the 2D map's MoveCentre, InitSprites and UpdateMapZoom (src/game/ui/ui_2dmap.cpp). The callees
# outside the unit are recording fakes in tests/unit_CUI2DMap_MoveCentre.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_MoveCentre src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_MoveCentre_KEEP
  __ZN8CUI2DMap10MoveCentreE21EUI2DMapMoveDirectionf   # CUI2DMap::MoveCentre(EUI2DMapMoveDirection, float)
  __ZN8CUI2DMap11InitSpritesEv   # CUI2DMap::InitSprites()
  __ZN8CUI2DMap13UpdateMapZoomEv   # CUI2DMap::UpdateMapZoom()
)
