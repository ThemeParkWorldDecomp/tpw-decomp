# Unit v51063: small accessors and static initialisers (src/game/HLSprite.cpp, src/game/park/LLCellMouse.cpp, src/game/park/LLCellRide.cpp, src/game/rides/FlyingRide.cpp, src/game/ui/intrface.cpp). The callees outside the unit are
# recording fakes in tests/unit_LLCR_GetRideCell.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCR_GetRideCell src/game/HLSprite.cpp src/game/park/LLCellMouse.cpp src/game/park/LLCellRide.cpp
    src/game/rides/FlyingRide.cpp src/game/ui/intrface.cpp platform/arena.cpp)
set(TPW_UNIT_LLCR_GetRideCell_KEEP
  __Z12DrawHLSpriteP9tagSCRIPT   # DrawHLSprite(tagSCRIPT*)
  __Z14FLY_SetCarSizeii   # FLY_SetCarSize(int, int)
  __Z15LLCR_SetTopLeft12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __Z16LLCR_GetRideCellii   # LLCR_GetRideCell(int, int)
  __Z17LLCC_SetTrackAnimP7CObjectb   # LLCC_SetTrackAnim(CObject*, bool)
  __Z18FLY_SetMotionScaleii   # FLY_SetMotionScale(int, int)
  __Z18LLCCM_DestroyQMeshj   # LLCCM_DestroyQMesh(unsigned int)
  __Z19LLCR_GetWidthHeightPiS_   # LLCR_GetWidthHeight(int*, int*)
  __Z20ESP_EnableSpriteDrawv   # ESP_EnableSpriteDraw()
  __Z20__sinit_intrface_cppv   # __sinit_intrface_cpp()
  __Z21ESP_DisableSpriteDrawv   # ESP_DisableSpriteDraw()
  __Z21LLCM_GetCurrentObjectv   # LLCM_GetCurrentObject()
  __Z22ESP_GetNumberOfSprites11ESP_Sprites   # ESP_GetNumberOfSprites(ESP_Sprites)
  __Z23LLCCM_DestroyKTrackMeshj   # LLCCM_DestroyKTrackMesh(unsigned int)
  __Z23LLCCM_DestroyWTrackMeshj   # LLCCM_DestroyWTrackMesh(unsigned int)
  __Z23__sinit_LLCellMouse_cppv   # __sinit_LLCellMouse_cpp()
  __Z24LLCM_GetIllegalPlacementv   # LLCM_GetIllegalPlacement()
  __Z25LLCCM_DestroyHoardingMeshj   # LLCCM_DestroyHoardingMesh(unsigned int)
  __Z25LLCR_GetRideCellDirectionii   # LLCR_GetRideCellDirection(int, int)
  __Z26ESP_GetNumberOfCustomAnims11ESP_Spritesi   # ESP_GetNumberOfCustomAnims(ESP_Sprites, int)
  _gIntrfaceDC1D8
  _gIntrfaceDC1DC
  _gLLCCDB74
  _gLLCCDB78
)
