set(TPW_UNIT_HFieldFrame src/engine/Gamut.cpp src/engine/tb_hfield.cpp src/engine/TextureCache.cpp platform/arena.cpp)
set(TPW_UNIT_HFieldFrame_KEEP
  __ZN13CTextureCache7ExecuteEv   # CTextureCache::Execute()
  __ZN13CTextureCache7RefreshEv   # CTextureCache::Refresh()
  __ZN6CGamut11GetVerticesEv   # CGamut::GetVertices()
  __ZN7CHField16DivideAndConquerEv   # CHField::DivideAndConquer()
  __ZN7CHField7PrepareEv   # CHField::Prepare()
  _gFrequencyIndex
  _gTileOverlays
)
