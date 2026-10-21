set(TPW_UNIT_TextureFrame src/engine/TextureCache.cpp)
set(TPW_UNIT_TextureFrame_KEEP
  __ZN12CTextureLink11PageUpdatedEv   # CTextureLink::PageUpdated()
  __ZN13CTextureCache12ReInitializeEi   # CTextureCache::ReInitialize(int)
  __ZN13CTextureCache12UploadToVRAMEP12CTextureLinkP14SAvailableQuadP13CTextureEntry   # CTextureCache::UploadToVRAM(CTextureLink*, SAvailableQuad*, CTextureEntry*)
  __ZN13CTextureCache20GenerateTexturePagesEv   # CTextureCache::GenerateTexturePages()
  __ZN13CTextureCache25DumpTCStreamToLog_ExecuteEv   # CTextureCache::DumpTCStreamToLog_Execute()
  __ZN13CTextureCache25DumpTCStreamToLog_PrepareEP13CTextureEntry   # CTextureCache::DumpTCStreamToLog_Prepare(CTextureEntry*)
  __ZN13CTextureCache7PrepareEP13CTextureEntryf   # CTextureCache::Prepare(CTextureEntry*, float)
  _gTexturePageUpdates
)
