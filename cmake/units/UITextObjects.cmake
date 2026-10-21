# The text draw objects (unit w35c). The sprite draw object under them and the rectangles are real; the text formatter, the font parameters,
# the render area helpers, the engine's sprite surfaces, the map and the draw object base class are fakes in the unit file, which also uses
# the font table's real classes (src/game/ui/InterfaceFont.cpp).
set(TPW_UNIT_UITextObjects src/game/ui/InterfaceTextObjectSprite.cpp src/game/ui/ui_textmanager.cpp
    src/game/ui/InterfaceDrawObjectSprite.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp
    src/game/ui/UI_Base.cpp src/game/ui/InterfaceFont.cpp)
set(TPW_UNIT_UITextObjects_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN13InterfaceFont7ReleaseEv   # InterfaceFont::Release()
  __ZN13InterfaceFont9GetHeightEv   # InterfaceFont::GetHeight()
  __ZN13InterfaceFontC1Ev   # InterfaceFont::InterfaceFont()
  __ZN13InterfaceFontD1Ev   # InterfaceFont::~InterfaceFont()
  __ZN14textobj_callee10CursorDtorEPvs   # textobj_callee::CursorDtor(void*, short)
  __ZN14textobj_callee6DeleteEPv   # textobj_callee::Delete(void*)
  __ZN20InterfaceFontWrapperC1EP7TbIFont   # InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*)
  __ZN20InterfaceFontWrapperD1Ev   # InterfaceFontWrapper::~InterfaceFontWrapper()
  __ZN24InterfaceTitleTextObject14UpdateTexturesEPDs   # InterfaceTitleTextObject::UpdateTextures(char16_t*)
  __ZN24InterfaceTitleTextObjectC1EPv   # InterfaceTitleTextObject::InterfaceTitleTextObject(void*)
  __ZN24InterfaceTitleTextObjectD1Ev   # InterfaceTitleTextObject::~InterfaceTitleTextObject()
  __ZN25InterfaceDrawObjectSprite14CreateTexturesEPDs   # InterfaceDrawObjectSprite::CreateTextures(char16_t*)
  __ZN25InterfaceDrawObjectSprite14UpdateTexturesEPDs   # InterfaceDrawObjectSprite::UpdateTextures(char16_t*)
  __ZN25InterfaceDrawObjectSprite18SetPreDrawCallbackEPFiR12TbRenderAreaE   # InterfaceDrawObjectSprite::SetPreDrawCallback(int (*)(TbRenderArea&))
  __ZN25InterfaceDrawObjectSprite19GetWritableDataAreaEPiS0_   # InterfaceDrawObjectSprite::GetWritableDataArea(int*, int*)
  __ZN25InterfaceDrawObjectSprite4DrawEsP8CoolRects   # InterfaceDrawObjectSprite::Draw(short, CoolRect*, short)
  __ZN25InterfaceDrawObjectSprite4ShowEs   # InterfaceDrawObjectSprite::Show(short)
  __ZN25InterfaceDrawObjectSprite6ChangeEjP8DOChange   # InterfaceDrawObjectSprite::Change(unsigned int, DOChange*)
  __ZN25InterfaceDrawObjectSprite7RefreshEv   # InterfaceDrawObjectSprite::Refresh()
  __ZN25InterfaceDrawObjectSpriteC1EPv   # InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void*)
  __ZN25InterfaceDrawObjectSpriteD1Ev   # InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite()
  __ZN25InterfaceTextObjectSprite14UpdateTexturesEPDs   # InterfaceTextObjectSprite::UpdateTextures(char16_t*)
  __ZN25InterfaceTextObjectSprite6ChangeEjP8DOChange   # InterfaceTextObjectSprite::Change(unsigned int, DOChange*)
  __ZN25InterfaceTextObjectSpriteC1EPv   # InterfaceTextObjectSprite::InterfaceTextObjectSprite(void*)
  __ZN25InterfaceTextObjectSpriteD1Ev   # InterfaceTextObjectSprite::~InterfaceTextObjectSprite()
  __ZN34InterfaceCursorPosTextObjectSprite5QueryEjl   # InterfaceCursorPosTextObjectSprite::Query(unsigned int, long)
  __ZN34InterfaceCursorPosTextObjectSpriteC1EPv   # InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void*)
  __ZN34InterfaceCursorPosTextObjectSpriteD1Ev   # InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite()
  __ZN8CoolRect14RectIntersectsEPS_   # CoolRect::RectIntersects(CoolRect*)
  __ZN8CoolRect3GetEPsS0_S0_S0_   # CoolRect::Get(short*, short*, short*, short*)
  __ZN8CoolRect3SetEssss   # CoolRect::Set(short, short, short, short)
  __ZN8CoolRect5ShiftEss   # CoolRect::Shift(short, short)
  __ZN8CoolRect6AdjustEssss   # CoolRect::Adjust(short, short, short, short)
  __ZN8CoolRect6MiddleEPsS0_   # CoolRect::Middle(short*, short*)
  __ZN8CoolRect7GetDimsEPsS0_   # CoolRect::GetDims(short*, short*)
  __ZN8CoolRect7PointInEss   # CoolRect::PointIn(short, short)
  __ZN8CoolRectC1Essss   # CoolRect::CoolRect(short, short, short, short)
  __ZN8CoolRectC1Ev   # CoolRect::CoolRect()
  __ZN8CoolRectD1Ev   # CoolRect::~CoolRect()
  _gUIHighPresets
  _gUIQueryRect
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
