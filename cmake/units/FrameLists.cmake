set(TPW_UNIT_FrameLists src/engine/RenderList.cpp src/engine/SpriteManager.cpp src/engine/tb_starfield.cpp
    platform/arena.cpp)
set(TPW_UNIT_FrameLists_KEEP
  __ZN10CStarfield5CloseEv   # CStarfield::Close()
  __ZN10CStarfield7ExecuteEv   # CStarfield::Execute()
  __ZN10CStarfield7PrepareEv   # CStarfield::Prepare()
  __ZN10CStarfieldD1Ev   # CStarfield::~CStarfield()
  __ZN11CRenderList15RenderByTextureEv   # CRenderList::RenderByTexture()
  __ZN11CRenderList5ResetEv   # CRenderList::Reset()
  __ZN11CRenderList6RenderEv   # CRenderList::Render()
  __ZN11CRenderList8AddEntryER7CMatrixS1_PP6CLightP9tag_sNodei   # CRenderList::AddEntry(CMatrix&, CMatrix&, CLight**, tag_sNode*, int)
  __ZN14CSpriteManager13RenderSpritesEv   # CSpriteManager::RenderSprites()
  __ZN14CSpriteManager14LoadBlobShadowEPc   # CSpriteManager::LoadBlobShadow(char*)
  __ZN14CSpriteManager16AddPartialSpriteEP15SSpriteGameData   # CSpriteManager::AddPartialSprite(SSpriteGameData*)
  __ZN14CSpriteManager20RenderPartialSpritesEv   # CSpriteManager::RenderPartialSprites()
  __ZN14CSpriteManager5ResetEv   # CSpriteManager::Reset()
  __ZN14CSpriteManager6RenderEv   # CSpriteManager::Render()
  __ZN14CSpriteManager7AddListEP11CSpriteList   # CSpriteManager::AddList(CSpriteList*)
  __ZN14CSpriteManager7PrepareEv   # CSpriteManager::Prepare()
  __ZN14CSpriteManager9AddSpriteEP15SSpriteGameData   # CSpriteManager::AddSprite(SSpriteGameData*)
  _gHotSprite
  _gRenderFaces
  _gRenderGroups
  _gSpriteGroups
  _gStarfield
)
