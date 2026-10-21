# The regular sprite draw path (unit w41a): the new sources under the real sprite manager, DrawSprite and AddPoly_TLCTEX, the clipper and the
# texture entry. The unit file includes calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_SpriteDraw src/engine/SpriteData.cpp src/engine/EngineUtil.cpp src/engine/DrawPrimitive.cpp
    src/engine/tb_error.cpp src/engine/SpriteManager.cpp src/engine/Clipping.cpp src/engine/TextureEntry.cpp
    platform/arena.cpp)
set(TPW_UNIT_SpriteDraw_KEEP
  __Z14RotateVertex_YP7SVertexfS0_   # RotateVertex_Y(SVertex*, float, SVertex*)
  __Z14RotateVertex_ZP7SVertexf   # RotateVertex_Z(SVertex*, float)
  __Z14VertexClip_TopP11SFVF_TLCTEXS0_S0_i   # VertexClip_Top(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z15VertexClip_BackP11SFVF_TLCTEXS0_S0_i   # VertexClip_Back(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z15VertexClip_LeftP11SFVF_TLCTEXS0_S0_i   # VertexClip_Left(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z16VertexClip_FrontP11SFVF_TLCTEXS0_S0_i   # VertexClip_Front(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z16VertexClip_RightP11SFVF_TLCTEXS0_S0_i   # VertexClip_Right(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __Z17InterpolateColourjjj   # InterpolateColour(unsigned int, unsigned int, unsigned int)
  __Z17VertexClip_BottomP11SFVF_TLCTEXS0_S0_i   # VertexClip_Bottom(SFVF_TLCTEX*, SFVF_TLCTEX*, SFVF_TLCTEX*, int)
  __ZN11SFVF_TLCTEX10SetDiffuseEj   # SFVF_TLCTEX::SetDiffuse(unsigned int)
  __ZN11SFVF_TLCTEX5SetUVEiff   # SFVF_TLCTEX::SetUV(int, float, float)
  __ZN11SFVF_TLCTEX9ReturnPtrEii   # SFVF_TLCTEX::ReturnPtr(int, int)
  __ZN11SFVF_TLCTEX9SetVertexE7SVertex   # SFVF_TLCTEX::SetVertex(SVertex)
  __ZN13CTextureEntry11GetBestSizeEv   # CTextureEntry::GetBestSize()
  __ZN13CTextureEntry15FlushEntryFlagsEv   # CTextureEntry::FlushEntryFlags()
  __ZN13CTextureEntry15GetBlendingModeEv   # CTextureEntry::GetBlendingMode()
  __ZN13CTextureEntry15StoreAsWrappingEv   # CTextureEntry::StoreAsWrapping()
  __ZN13CTextureEntry16GetCorrectedDataEv   # CTextureEntry::GetCorrectedData()
  __ZN13CTextureEntry4LockEv   # CTextureEntry::Lock()
  __ZN13CTextureEntry5CloseEv   # CTextureEntry::Close()
  __ZN13CTextureEntry6CreateEii   # CTextureEntry::Create(int, int)
  __ZN13CTextureEntry6UnlockEv   # CTextureEntry::Unlock()
  __ZN13CTextureEntry7GetDataEv   # CTextureEntry::GetData()
  __ZN13CTextureEntry7SetSizeEii   # CTextureEntry::SetSize(int, int)
  __ZN13CTextureEntry8BestSizeEi   # CTextureEntry::BestSize(int)
  __ZN13CTextureEntryC1Ev   # CTextureEntry::CTextureEntry()
  __ZN13CTextureEntryD1Ev   # CTextureEntry::~CTextureEntry()
  __ZN14CDrawPrimitive10DrawSpriteEP21SSpriteDrawParametersi   # CDrawPrimitive::DrawSprite(SSpriteDrawParameters*, int)
  __ZN14CDrawPrimitive14AddPoly_TLCTEXEP11SFVF_TLCTEXiiP12ClipCodeTypei   # CDrawPrimitive::AddPoly_TLCTEX(SFVF_TLCTEX*, int, int, ClipCodeType*, int)
  __ZN14CDrawPrimitive9AddSpriteEP21SSpriteDrawParametersi   # CDrawPrimitive::AddSprite(SSpriteDrawParameters*, int)
  __ZN14CSpriteManager13RenderSpritesEv   # CSpriteManager::RenderSprites()
  __ZN14CSpriteManager14LoadBlobShadowEPc   # CSpriteManager::LoadBlobShadow(char*)
  __ZN14CSpriteManager16AddPartialSpriteEP15SSpriteGameData   # CSpriteManager::AddPartialSprite(SSpriteGameData*)
  __ZN14CSpriteManager20RenderPartialSpritesEv   # CSpriteManager::RenderPartialSprites()
  __ZN14CSpriteManager5ResetEv   # CSpriteManager::Reset()
  __ZN14CSpriteManager6RenderEv   # CSpriteManager::Render()
  __ZN14CSpriteManager7AddListEP11CSpriteList   # CSpriteManager::AddList(CSpriteList*)
  __ZN14CSpriteManager7PrepareEv   # CSpriteManager::Prepare()
  __ZN14CSpriteManager9AddSpriteEP15SSpriteGameData   # CSpriteManager::AddSprite(SSpriteGameData*)
  __ZN15SSpriteGameData10AddRegularEv   # SSpriteGameData::AddRegular()
  __ZN15SSpriteGameData11CalcRegularEP21SSpriteDrawParameters   # SSpriteGameData::CalcRegular(SSpriteDrawParameters*)
  __ZN15SSpriteGameData11DrawRegularEv   # SSpriteGameData::DrawRegular()
  __ZN15SSpriteGameData13SetupVerticesEP7SVertexff   # SSpriteGameData::SetupVertices(SVertex*, float, float)
  __ZN15SSpriteGameData7PrepareEv   # SSpriteGameData::Prepare()
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
  _gHotSprite
  _gSpriteGroups
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_SpriteDraw_OWN_CALLLOG ON)
