set(TPW_UNIT_CursorClasses src/game/ui/InterfaceBFCursor.cpp src/bullfrog/AndyGLL.cpp src/bullfrog/LbDrawText.cpp
    src/bullfrog/Lbgfsave.cpp src/bullfrog/TbRenderArea.cpp src/game/ui/UIInterface.cpp src/game/ui/ui_main.cpp
    src/game/ui/InterfaceCursor.cpp src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceDrawObject.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp src/bullfrog/TbPixFmt.cpp
    src/game/Light.cpp platform/arena.cpp)
set(TPW_UNIT_CursorClasses_KEEP
  _TbPixFormat16
  _TbPixFormat24
  _TbPixFormat32
  _TbPixFormat8
  __Z10LoadBFCursPKc   # LoadBFCurs(char const*)
  __Z11InitLobbyUIv   # InitLobbyUI()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21LbPalette_FromCompactP9TbPalettePKhj   # LbPalette_FromCompact(TbPalette*, unsigned char const*, unsigned int)
  __Z23InitialiseCOLOURCursorsv   # InitialiseCOLOURCursors()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN10TbMaskInfo3SetEjj   # TbMaskInfo::Set(unsigned int, unsigned int)
  __ZN11CursorTable10DeAllocateEv   # CursorTable::DeAllocate()
  __ZN11CursorTable8AllocateEt   # CursorTable::Allocate(unsigned short)
  __ZN11CursorTable8GetEntryEt   # CursorTable::GetEntry(unsigned short)
  __ZN11CursorTable8SetEntryEtP15InterfaceCursor   # CursorTable::SetEntry(unsigned short, InterfaceCursor*)
  __ZN11CursorTableC1Ev   # CursorTable::CursorTable()
  __ZN11CursorTableD1Ev   # CursorTable::~CursorTable()
  __ZN11TbPixelCaps3SetERK11TbPixFormat   # TbPixelCaps::Set(TbPixFormat const&)
  __ZN11TbTGALoader10ValidImageERN3bfl10FileSystem5CFileE   # TbTGALoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN11TbTGALoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbTGALoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTGALoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbTGALoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTGALoaderC1Ev   # TbTGALoader::TbTGALoader()
  __ZN11TbTGALoaderD1Ev   # TbTGALoader::~TbTGALoader()
  __ZN11UIInterface9InitLobbyEv   # UIInterface::InitLobby()
  __ZN12TbRenderArea10FreeMemoryEv   # TbRenderArea::FreeMemory()
  __ZN12TbRenderArea11AllocMemoryEv   # TbRenderArea::AllocMemory()
  __ZN12TbRenderAreaC1EPK9TbSurface   # TbRenderArea::TbRenderArea(TbSurface const*)
  __ZN12TbRenderAreaC1ERK6TbSizeRK11TbPixFormatPhj   # TbRenderArea::TbRenderArea(TbSize const&, TbPixFormat const&, unsigned char*, unsigned int)
  __ZN12TbRenderAreaC1Ev   # TbRenderArea::TbRenderArea()
  __ZN15InterfaceCursor10GetVisibleEv   # InterfaceCursor::GetVisible()
  __ZN15InterfaceCursor3UseEs   # InterfaceCursor::Use(short)
  __ZN15InterfaceCursor4DrawEss   # InterfaceCursor::Draw(short, short)
  __ZN15InterfaceCursor4HideEv   # InterfaceCursor::Hide()
  __ZN15InterfaceCursor4ShowEv   # InterfaceCursor::Show()
  __ZN15InterfaceCursor6SetHotEcc   # InterfaceCursor::SetHot(char, char)
  __ZN15InterfaceCursor7AnimateEi   # InterfaceCursor::Animate(int)
  __ZN15InterfaceCursor7SetDataEPvc   # InterfaceCursor::SetData(void*, char)
  __ZN15InterfaceCursorC1Ev   # InterfaceCursor::InterfaceCursor()
  __ZN15InterfaceCursorD1Ev   # InterfaceCursor::~InterfaceCursor()
  __ZN17DrawObjectLibrary13AddDrawObjectEP19InterfaceDrawObjectPc   # DrawObjectLibrary::AddDrawObject(InterfaceDrawObject*, char*)
  __ZN17DrawObjectLibrary14FindDrawObjectEPc   # DrawObjectLibrary::FindDrawObject(char*)
  __ZN17DrawObjectLibrary14FindDrawObjectEj   # DrawObjectLibrary::FindDrawObject(unsigned int)
  __ZN17DrawObjectLibrary9RemoveAllEs   # DrawObjectLibrary::RemoveAll(short)
  __ZN17InterfaceBFCursor10GetVisibleEv   # InterfaceBFCursor::GetVisible()
  __ZN17InterfaceBFCursor3UseEs   # InterfaceBFCursor::Use(short)
  __ZN17InterfaceBFCursor4HideEv   # InterfaceBFCursor::Hide()
  __ZN17InterfaceBFCursor4ShowEv   # InterfaceBFCursor::Show()
  __ZN17InterfaceBFCursor8LoadDataEPKc   # InterfaceBFCursor::LoadData(char const*)
  __ZN17InterfaceBFCursorC1Ev   # InterfaceBFCursor::InterfaceBFCursor()
  __ZN17InterfaceBFCursorD1Ev   # InterfaceBFCursor::~InterfaceBFCursor()
  __ZN19InterfaceDrawObject10InstanceMeEv   # InterfaceDrawObject::InstanceMe()
  __ZN19InterfaceDrawObject12SetFrameBaseEs   # InterfaceDrawObject::SetFrameBase(short)
  __ZN19InterfaceDrawObject19GetWritableDataAreaEPiS0_   # InterfaceDrawObject::GetWritableDataArea(int*, int*)
  __ZN19InterfaceDrawObject4DrawEsP8CoolRects   # InterfaceDrawObject::Draw(short, CoolRect*, short)
  __ZN19InterfaceDrawObject4DrawEsP8CoolRectsPDs   # InterfaceDrawObject::Draw(short, CoolRect*, short, char16_t*)
  __ZN19InterfaceDrawObject4ShowEs   # InterfaceDrawObject::Show(short)
  __ZN19InterfaceDrawObject5QueryEjl   # InterfaceDrawObject::Query(unsigned int, long)
  __ZN19InterfaceDrawObject6ChangeEjP8DOChange   # InterfaceDrawObject::Change(unsigned int, DOChange*)
  __ZN19InterfaceDrawObject7RefreshEv   # InterfaceDrawObject::Refresh()
  __ZN19InterfaceDrawObject8SetFrameEs   # InterfaceDrawObject::SetFrame(short)
  __ZN19InterfaceDrawObjectC1EPv   # InterfaceDrawObject::InterfaceDrawObject(void*)
  __ZN19InterfaceDrawObjectD1Ev   # InterfaceDrawObject::~InterfaceDrawObject()
  __ZN19TbGraphicFileLoader10ReadHeaderEPKcS1_P12TbRenderArea   # TbGraphicFileLoader::ReadHeader(char const*, char const*, TbRenderArea*)
  __ZN19TbGraphicFileLoader10ReadHeaderERN3bfl10FileSystem5CFileEP12TbRenderArea   # TbGraphicFileLoader::ReadHeader(bfl::FileSystem::CFile&, TbRenderArea*)
  __ZN19TbGraphicFileLoader4LoadERK12TbRenderAreaPKcS4_PS0_   # TbGraphicFileLoader::Load(TbRenderArea const&, char const*, char const*, TbRenderArea*)
  __ZN19TbGraphicFileLoader4LoadERK12TbRenderAreaRN3bfl10FileSystem5CFileEPS0_   # TbGraphicFileLoader::Load(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea*)
  __ZN19TbGraphicFileLoader7IsValidERN3bfl10FileSystem5CFileE   # TbGraphicFileLoader::IsValid(bfl::FileSystem::CFile&)
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRects   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short)
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRectsPDs   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short, char16_t*)
  __ZN23InterfaceDrawEngineMesh4ShowEs   # InterfaceDrawEngineMesh::Show(short)
  __ZN23InterfaceDrawEngineMeshC1EPv   # InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void*)
  __ZN23InterfaceDrawEngineMeshD1Ev   # InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh()
  __ZN33InterfaceDrawEngineResizableFrameC1EPv   # InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void*)
  __ZN33InterfaceDrawEngineResizableFrameD1Ev   # InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame()
  __ZN34InterfaceDrawEngineResizableSliderC1EPv   # InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void*)
  __ZN34InterfaceDrawEngineResizableSliderD1Ev   # InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider()
  __ZN3bfl10FileSystem15_TbBufferedFileD1Ev   # bfl::FileSystem::_TbBufferedFile::~_TbBufferedFile()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
  __ZN7_TgaGLL10TGALoadRLEERN3bfl10FileSystem5CFileE   # _TgaGLL::TGALoadRLE(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL12ReadNextLineERN3bfl10FileSystem5CFileE   # _TgaGLL::ReadNextLine(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL13ReadFirstLineERN3bfl10FileSystem5CFileE   # _TgaGLL::ReadFirstLine(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL14ReadHeaderInfoERN3bfl10FileSystem5CFileEP11TbTGALoader   # _TgaGLL::ReadHeaderInfo(bfl::FileSystem::CFile&, TbTGALoader*)
  __ZN7_TgaGLL15ReverseScanLineEPh   # _TgaGLL::ReverseScanLine(unsigned char*)
  __ZN7_TgaGLL19GetSourceLineLengthEv   # _TgaGLL::GetSourceLineLength()
  __ZN7_TgaGLL8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileEP11TbTGALoader   # _TgaGLL::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbTGALoader*)
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
  __ZN8_AndyGLL6DoReadERK12TbRenderAreaRN3bfl10FileSystem5CFileEP19TbGraphicFileLoader   # _AndyGLL::DoRead(TbRenderArea const&, bfl::FileSystem::CFile&, TbGraphicFileLoader*)
  __ZN8_AndyGLL9WriteLineEv   # _AndyGLL::WriteLine()
  __ZNK12TbRenderArea10GetMemSizeEv   # TbRenderArea::GetMemSize() const
  __ZNK12TbRenderArea13SetupClientRAERS_RK6TbRect   # TbRenderArea::SetupClientRA(TbRenderArea&, TbRect const&) const
  __ZNK12TbRenderArea13SetupClientRAERS_RK7TbPointRK6TbSize   # TbRenderArea::SetupClientRA(TbRenderArea&, TbPoint const&, TbSize const&) const
  _gBFCursorFolder
  _gBFCursorFolderSet
  _gDrawObjectLibrary
  _gTbLoadFailed
  _gTbReadHeaderFailed
  _gUIHeap
  _gUIHighPresets
  _gUILight
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __ZN12_GLOBAL__N_113sUILightSetupE   # sUILightSetup (static object)
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
  __Z19UI_UseInterfaceSizeb   # UI_UseInterfaceSize(bool)
  _gUIRootWindow
)
