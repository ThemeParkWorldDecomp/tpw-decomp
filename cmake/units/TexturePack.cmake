set(TPW_UNIT_TexturePack src/engine/TextureCache.cpp)
set(TPW_UNIT_TexturePack_KEEP
  __ZN13CTextureCache12PackTexturesEPP13CTextureEntry   # CTextureCache::PackTextures(CTextureEntry**)
  __ZN13CTextureCache26ArrangeNonwrappingTexturesEPP13CTextureEntryP14SAvailableQuadPiP12CTextureBasei   # CTextureCache::ArrangeNonwrappingTextures(CTextureEntry**, SAvailableQuad*, int*, CTextureBase*, int)
  _gTextureCutDowns
  _gTextureQuadsPending
  _gTextureQuadsPlaced
  _gTextureSizeSteps
)
