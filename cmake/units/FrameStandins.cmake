set(TPW_UNIT_FrameStandins src/engine/DrawPrimitiveOpenGL.cpp src/engine/OpenGLTexture.cpp src/engine/TextureCache.cpp
    src/engine/tb_profiler.cpp src/engine/Rain.cpp)
set(TPW_UNIT_FrameStandins_KEEP
  __ZN13CTextureCache23Entry_Specific_CompressEPP13CTextureEntryiii   # CTextureCache::Entry_Specific_Compress(CTextureEntry**, int, int, int)
  __ZN13CTextureCache25BalanceCache_ClearTextureEP12CTextureLink   # CTextureCache::BalanceCache_ClearTexture(CTextureLink*)
  __ZN14COpenGLTexture8DownloadEP13CTextureEntryii   # COpenGLTexture::Download(CTextureEntry*, int, int)
  __ZN20COpenGLDrawPrimitiveD1Ev   # COpenGLDrawPrimitive::~COpenGLDrawPrimitive()
  __ZN5CRain16RenderLightning2EP10_D3DVECTORS1_   # CRain::RenderLightning2(_D3DVECTOR*, _D3DVECTOR*)
  __ZN5CRain6RenderEv   # CRain::Render()
  __ZN5CRain7PrepareEv   # CRain::Prepare()
  __ZN9CParticle13PlotParticlesEv   # CParticle::PlotParticles()
  __ZN9CParticle7PrepareEv   # CParticle::Prepare()
)
