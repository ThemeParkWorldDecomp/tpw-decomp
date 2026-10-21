# The lobby's small leaves part 6 (unit w75): CGame::ExitLevel, CInGame::Start, CursorLibrary::RemoveAll and FuckOffUIProgressBar, the image
# control's destructor, DestroyEngineMesh and DestroyMeshInstance, StripRideWrapping. Everything they call is a fake in the unit file.
set(TPW_UNIT_LobbyLeaves6 src/game/Game.cpp src/game/InGame.cpp src/game/ui/InterfaceCursor.cpp
    src/game/ui/ui_progressBar.cpp src/game/ui/InterfaceImage.cpp src/game/TPInstance.cpp src/game/TPMesh.cpp
    src/game/rides/RidePlacement.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp
    platform/arena.cpp)
set(TPW_UNIT_LobbyLeaves6_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z17DestroyEngineMeshP9tag_sMesh   # DestroyEngineMesh(tag_sMesh*)
  __Z17StripRideWrappingP16tag_sRideAnimSetP9tag_sNodefPvP8s3DPoint   # StripRideWrapping(tag_sRideAnimSet*, tag_sNode*, float, void*, s3DPoint*)
  __Z19DestroyMeshInstanceP9tag_sMeshP9sHeapInfoj   # DestroyMeshInstance(tag_sMesh*, sHeapInfo*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20FuckOffUIProgressBarv   # FuckOffUIProgressBar()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN13CursorLibrary9RemoveAllEv   # CursorLibrary::RemoveAll()
  __ZN14InterfaceImageD1Ev   # InterfaceImage::~InterfaceImage()
  __ZN5CGame9ExitLevelEv   # CGame::ExitLevel()
  __ZN7CInGame5StartEv   # CInGame::Start()
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
  _gCursorLibrary
  _gUIHighPresets
  _gUIProgressBar
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
