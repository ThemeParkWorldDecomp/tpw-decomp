set(TPW_UNIT_RainRender src/engine/DrawPrimitive.cpp src/engine/EngineUtil.cpp src/engine/Rain.cpp
    src/engine/Clipping.cpp)
set(TPW_UNIT_RainRender_KEEP
  __Z13GeneratePointP10_D3DVECTORS0_S0_f   # GeneratePoint(_D3DVECTOR*, _D3DVECTOR*, _D3DVECTOR*, float)
  __Z13GetCycleCountv   # GetCycleCount()
  __Z14VertexClip_TopP11SFVF_TLCTEXS0_S0_i   # VertexClip_Top(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z15VertexClip_BackP11SFVF_TLCTEXS0_S0_i   # VertexClip_Back(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z15VertexClip_LeftP11SFVF_TLCTEXS0_S0_i   # VertexClip_Left(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z16VertexClip_FrontP11SFVF_TLCTEXS0_S0_i   # VertexClip_Front(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z16VertexClip_RightP11SFVF_TLCTEXS0_S0_i   # VertexClip_Right(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z17InterpolateColourjjj   # InterpolateColour(unsigned int, unsigned int, unsigned int)
  __Z17VertexClip_BottomP11SFVF_TLCTEXS0_S0_i   # VertexClip_Bottom(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __ZN10SLightning4InitE10_D3DVECTORS0_ii   # SLightning::Init(_D3DVECTOR, _D3DVECTOR, int, int)
  __ZN14CDrawPrimitive10DrawSpriteEP21SSpriteDrawParametersi   # CDrawPrimitive::DrawSprite(SSpriteDrawParameters*, int)
  __ZN14CDrawPrimitive14AddPoly_TLCTEXEP11SFVF_TLCTEXiiP12ClipCodeTypei   # CDrawPrimitive::AddPoly_TLCTEX(SFVF_TLCTEX*, int, int, ClipCodeType*, int)
  __ZN5CRain12AddLightningE10_D3DVECTORS0_ii   # CRain::AddLightning(_D3DVECTOR, _D3DVECTOR, int, int)
  __ZN5CRain12__RenderRainEfff   # CRain::__RenderRain(float, float, float)
  __ZN5CRain12__RenderSnowEfff   # CRain::__RenderSnow(float, float, float)
  __ZN5CRain16__LightningFlashEv   # CRain::__LightningFlash()
  __ZN5CRain17__RenderLightningEv   # CRain::__RenderLightning()
  __ZN5CRain18NewRenderLightningEP10_D3DVECTORS1_   # CRain::NewRenderLightning(_D3DVECTOR*, _D3DVECTOR*)
  __ZN5CRain19AddLightningSectionEP10_D3DVECTORS1_   # CRain::AddLightningSection(_D3DVECTOR*, _D3DVECTOR*)
  __ZN9CClipping12CPP_TestDataEPhPvS0_PKviij   # CClipping::CPP_TestData(unsigned char*, void*, unsigned char*, void const*, int, int, unsigned int)
  __ZN9CClipping16CheckBoundingBoxERK11sAlignedBoxR7CMatrix   # CClipping::CheckBoundingBox(sAlignedBox const&, CMatrix&)
  __ZN9CClipping8ClipPolyEiP11SFVF_TLCTEXP14VertexClipListRii   # CClipping::ClipPoly(int, SFVF_TLCTEX*, VertexClipList*, int&, int)
  __ZN9CClipping8ClipPolyEiP11SFVF_TLCTEXS1_Rii   # CClipping::ClipPoly(int, SFVF_TLCTEX*, SFVF_TLCTEX*, int&, int)
  __ZN9CClipping8ClipPolyEiPtP11SFVF_TLCTEXRii   # CClipping::ClipPoly(int, unsigned short*, SFVF_TLCTEX*, int&, int)
  _gClipBytesLeft
  _gClipFraction
  _gClipInsideDistance
  _gClipOutsideDistance
  _gClipStep
)
