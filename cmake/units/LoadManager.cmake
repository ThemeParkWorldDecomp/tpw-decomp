set(TPW_UNIT_LoadManager src/engine/CsTexture.cpp src/engine/TextureLoadManager.cpp platform/arena.cpp)
set(TPW_UNIT_LoadManager_KEEP
  __Z24DelayedLoad_LoadTexturesv   # DelayedLoad_LoadTextures()
  __ZN19CTextureLoadManager13RemoveTextureEP13CTextureEntry   # CTextureLoadManager::RemoveTexture(CTextureEntry*)
  __ZN9CsTexture13GetFrameCountEv   # CsTexture::GetFrameCount()
  __ZN9CsTexture19LoadTextureFromDiskEv   # CsTexture::LoadTextureFromDisk()
  __ZN9CsTexture4LoadEP13CTextureEntry   # CsTexture::Load(CTextureEntry*)
  __ZN9CsTexture4LoadEP8sTextureP12sTextureInfo   # CsTexture::Load(sTexture*, sTextureInfo*)
  __ZN9CsTexture4LoadEPc   # CsTexture::Load(char*)
  __ZN9CsTexture5CloseEv   # CsTexture::Close()
  __ZN9CsTexture5ResetEv   # CsTexture::Reset()
  __ZN9CsTexture6ReLoadEPS_   # CsTexture::ReLoad(CsTexture*)
  __ZN9CsTexturedaEPv   # CsTexture::operator delete[](void*)
  __ZN9CsTexturedlEPv   # CsTexture::operator delete(void*)
  __ZN9CsTexturenaEm   # CsTexture::operator new[](unsigned long)
  __ZN9CsTexturenwEm   # CsTexture::operator new(unsigned long)
  _gDelayedLoadCount
  _gDelayedLoads
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LoadManager_OWN_CALLLOG ON)
