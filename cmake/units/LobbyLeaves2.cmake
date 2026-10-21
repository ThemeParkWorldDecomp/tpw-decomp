# The lobby's small leaves (unit w58): the face normals (RecalculateInvalid*FaceNormals), the matrix rotations by axis order, TbRect::Bounding
# and the end of the interface (DeInitGeneralUI, UIInterface::DeInitGeneral, which sit in ui_main_meshes.cpp with the rest of that file, so the
# sources of the UIStart unit are linked as well and the unit file fakes what they call). The cursor table, the heaps and the UI system are fakes.
set(TPW_UNIT_LobbyLeaves2 src/game/rides/RideAnim.cpp src/game/LMatrix.cpp src/bullfrog/LbRect.cpp
    src/game/ui/InterfaceFont.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp
    src/game/ui/InterfaceCursor.cpp src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceDrawObject.cpp
    src/game/ui/UIInterface.cpp src/game/ui/ui_main.cpp src/game/TextureTable.cpp src/game/TPGlobals.cpp
    platform/arena.cpp)
set(TPW_UNIT_LobbyLeaves2_KEEP
  __Z10IsMiscCharP10sFileParse   # IsMiscChar(sFileParse*)
  __Z12IsWhiteSpaceP10sFileParse   # IsWhiteSpace(sFileParse*)
  __Z13InitGeneralUIv   # InitGeneralUI()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14SkipWhitespaceP10sFileParse   # SkipWhitespace(sFileParse*)
  __Z15DeInitGeneralUIv   # DeInitGeneralUI()
  __Z15LoadAllUIMeshesv   # LoadAllUIMeshes()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21LoadLocalisedUIMeshesv   # LoadLocalisedUIMeshes()
  __Z21XRotateLMatrixNoTransP7sMatrixff   # XRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z21YRotateLMatrixNoTransP7sMatrixff   # YRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z21ZRotateLMatrixNoTransP7sMatrixff   # ZRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z23RotateLMatrixXYZNoTransP7sMatrix13eRotateParamsPf   # RotateLMatrixXYZNoTrans(sMatrix*, eRotateParams, float*)
  __Z23TPResource_AddFrameMeshN3bfl10FileSystem9CFileInfoERNS0_12CFileStorageERK23TbDynamicStringTemplateIcE   # TPResource_AddFrameMesh(bfl::FileSystem::CFileInfo, bfl::FileSystem::CFileStorage&, TbDynamicStringTemplate<char> const&)
  __Z24TPResource_AddSliderMeshN3bfl10FileSystem9CFileInfoERNS0_12CFileStorageERK23TbDynamicStringTemplateIcE   # TPResource_AddSliderMesh(bfl::FileSystem::CFileInfo, bfl::FileSystem::CFileStorage&, TbDynamicStringTemplate<char> const&)
  __Z25RotateLMatrixXYZNoTransHiP7sMatrix13eRotateParamsPf   # RotateLMatrixXYZNoTransHi(sMatrix*, eRotateParams, float*)
  __Z28TPResource_AddMeshDrawObjectN3bfl10FileSystem9CFileInfoERNS0_12CFileStorageE23TbDynamicStringTemplateIcE   # TPResource_AddMeshDrawObject(bfl::FileSystem::CFileInfo, bfl::FileSystem::CFileStorage&, TbDynamicStringTemplate<char>)
  __Z29RecalculateInvalidFaceNormalsP15tag_sM3D2Header   # RecalculateInvalidFaceNormals(tag_sM3D2Header*)
  __Z33RecalculateInvalidNodeFaceNormalsP9tag_sNode   # RecalculateInvalidNodeFaceNormals(tag_sNode*)
  __Z8IsLetterP10sFileParse   # IsLetter(sFileParse*)
  __Z8IsNumberP10sFileParse   # IsNumber(sFileParse*)
  __Z9GetStringP10sFileParsePciiiii   # GetString(sFileParse*, char*, int, int, int, int, int)
  __Z9IsCommentP10sFileParse   # IsComment(sFileParse*)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11CursorTable10DeAllocateEv   # CursorTable::DeAllocate()
  __ZN11CursorTable8AllocateEt   # CursorTable::Allocate(unsigned short)
  __ZN11CursorTable8GetEntryEt   # CursorTable::GetEntry(unsigned short)
  __ZN11CursorTable8SetEntryEtP15InterfaceCursor   # CursorTable::SetEntry(unsigned short, InterfaceCursor*)
  __ZN11CursorTableC1Ev   # CursorTable::CursorTable()
  __ZN11CursorTableD1Ev   # CursorTable::~CursorTable()
  __ZN11UIInterface11InitGeneralEv   # UIInterface::InitGeneral()
  __ZN11UIInterface13DeInitGeneralEv   # UIInterface::DeInitGeneral()
  __ZN13InterfaceFont7ReleaseEv   # InterfaceFont::Release()
  __ZN13InterfaceFont9GetHeightEv   # InterfaceFont::GetHeight()
  __ZN13InterfaceFontC1Ev   # InterfaceFont::InterfaceFont()
  __ZN13InterfaceFontD1Ev   # InterfaceFont::~InterfaceFont()
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
  __ZN17clockTickFunction10tickytickyEv   # clockTickFunction::tickyticky()
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
  __ZN20InterfaceFontWrapperC1EP7TbIFont   # InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*)
  __ZN20InterfaceFontWrapperD1Ev   # InterfaceFontWrapper::~InterfaceFontWrapper()
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRects   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short)
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRectsPDs   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short, char16_t*)
  __ZN23InterfaceDrawEngineMesh4ShowEs   # InterfaceDrawEngineMesh::Show(short)
  __ZN23InterfaceDrawEngineMeshC1EPv   # InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void*)
  __ZN23InterfaceDrawEngineMeshD1Ev   # InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh()
  __ZN23LbTimeClockTickFunction10tickytickyEv   # LbTimeClockTickFunction::tickyticky()
  __ZN33InterfaceDrawEngineResizableFrameC1EPv   # InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void*)
  __ZN33InterfaceDrawEngineResizableFrameD1Ev   # InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame()
  __ZN34InterfaceDrawEngineResizableSliderC1EPv   # InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void*)
  __ZN34InterfaceDrawEngineResizableSliderD1Ev   # InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider()
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
  __ZN9TPGlobalsC1Ev   # TPGlobals::TPGlobals()
  __ZN9TPGlobalsD1Ev   # TPGlobals::~TPGlobals()
  __ZNK6TbRect12IntersectionERKS_   # TbRect::Intersection(TbRect const&) const
  __ZNK6TbRect8BoundingERKS_   # TbRect::Bounding(TbRect const&) const
  _gCursorTable
  _gDrawObjectLibrary
  _gTitleText
  _gUIButtonName
  _gUIClockTick
  _gUIFrameName
  _gUIGeneralWindowA
  _gUIGeneralWindowB
  _gUIGeneralWindowC
  _gUIHeap
  _gUIHighPresets
  _gUIPlainFrameName
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUISliderName
  _gUIUnitWidth
  _gUIWideAspect
)
