# Unit v5058: CUI2DMap::CreateMapSprites (src/game/ui/ui_2dmap.cpp). The callees outside the unit are recording fakes in
# tests/unit_CUI2DMap_CreateMapSprites.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_CreateMapSprites src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_CreateMapSprites_KEEP
  __ZN8CUI2DMap16CreateMapSpritesEv   # CUI2DMap::CreateMapSprites()
)
