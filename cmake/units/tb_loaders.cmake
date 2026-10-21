set(TPW_UNIT_tb_loaders src/engine/TextureLoadManager.cpp)
set(TPW_UNIT_tb_loaders_KEEP
  __Z17TGALoadAndConvertPcPiS0_iP9lARGBTypei   # TGALoadAndConvert(char*, int*, int*, int, lARGBType*, int)
  __Z19UncompressedConvertPvS_iiP9lARGBTypeiii   # UncompressedConvert(void*, void*, int, int, lARGBType*, int, int, int)
  __Z23TextureLoadManager_Initv   # TextureLoadManager_Init()
  __Z24TextureLoadManager_Closev   # TextureLoadManager_Close()
  __ZN10STGAHeader11LoadBumpmapEPc   # STGAHeader::LoadBumpmap(char*)
  __ZN10STGAHeader22ConvertToEmbossBumpmapEPh   # STGAHeader::ConvertToEmbossBumpmap(unsigned char*)
  __ZN10STGAHeader4LoadEPci   # STGAHeader::Load(char*, int)
  __ZN12STextureLoad5CloseEv   # STextureLoad::Close()
  __ZN12STextureLoadC1Ev   # STextureLoad::STextureLoad()
  __ZN19CTextureLoadManager11LoadTextureEPcS0_   # CTextureLoadManager::LoadTexture(char*, char*)
  __ZN19CTextureLoadManager4InitEv   # CTextureLoadManager::Init()
  __ZN19CTextureLoadManager5CloseEv   # CTextureLoadManager::Close()
  __ZN19CTextureLoadManager8LoadFileEPc   # CTextureLoadManager::LoadFile(char*)
  __ZN19CTextureLoadManagerC1Ev   # CTextureLoadManager::CTextureLoadManager()
  _gTextureAverageColour
  _gTextureBytesPerPixel
  _gTextureExtraPlanes
  _gTextureLoadFlags
  _gTextureWaveData
  _gei_TLoadManager
)
