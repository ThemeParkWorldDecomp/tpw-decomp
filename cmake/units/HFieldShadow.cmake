# Height field sky shadow pass and procedural texture loader.
set(TPW_UNIT_HFieldShadow src/engine/tb_hfield.cpp platform/arena.cpp)
set(TPW_UNIT_HFieldShadow_KEEP
  __ZN7CHField13DrawSkyShadowEv   # CHField::DrawSkyShadow()
  __ZN7CHField14CalcShadowLineEPhP7SVertexiii   # CHField::CalcShadowLine(unsigned char*, SVertex*, int, int, int)
  __ZN7CHField16LoadProcTexturesEPci   # CHField::LoadProcTextures(char*, int)
  _gShadowUInitialized
  _gShadowUScale
  _gShadowVInitialized
  _gShadowVScale
)
