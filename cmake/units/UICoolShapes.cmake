# The region shapes' virtual members (unit w31a, beyond its list: the shapes' vtables need them): nothing is faked but the allocator.
set(TPW_UNIT_UICoolShapes src/game/ui/CoolRegion.cpp src/game/ui/InterfaceKnob.cpp src/game/ui/InterfaceListBox.cpp
    src/game/ui/InterfaceListControl2.cpp src/game/ui/CoolRect.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UICoolShapes_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolCircle5ShiftEss   # CoolCircle::Shift(short, short)
  __ZN10CoolCircle6AdjustEssss   # CoolCircle::Adjust(short, short, short, short)
  __ZN10CoolCircle7PointInEss   # CoolCircle::PointIn(short, short)
  __ZN10CoolCircleC1Esss   # CoolCircle::CoolCircle(short, short, short)
  __ZN10CoolCircleD1Ev   # CoolCircle::~CoolCircle()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11CoolEllipse5ShiftEss   # CoolEllipse::Shift(short, short)
  __ZN11CoolEllipse6AdjustEssss   # CoolEllipse::Adjust(short, short, short, short)
  __ZN11CoolEllipse7PointInEss   # CoolEllipse::PointIn(short, short)
  __ZN11CoolEllipseC1Essss   # CoolEllipse::CoolEllipse(short, short, short, short)
  __ZN11CoolEllipseD1Ev   # CoolEllipse::~CoolEllipse()
  __ZN13InterfaceKnob15SetMinMaxAnglesEPiS0_   # InterfaceKnob::SetMinMaxAngles(int*, int*)
  __ZN16InterfaceListBox11SetListRectEssss   # InterfaceListBox::SetListRect(short, short, short, short)
  __ZN21InterfaceListControl210SetColumnsEs   # InterfaceListControl2::SetColumns(short)
  __ZN21InterfaceListControl211SetListRectEssss   # InterfaceListControl2::SetListRect(short, short, short, short)
  __ZN21InterfaceListControl212SetColumnPosEsss   # InterfaceListControl2::SetColumnPos(short, short, short)
  __ZN21InterfaceListControl213GetZOfEntriesEv   # InterfaceListControl2::GetZOfEntries()
  __ZN8CoolPoly5ShiftEss   # CoolPoly::Shift(short, short)
  __ZN8CoolPoly6AdjustEssss   # CoolPoly::Adjust(short, short, short, short)
  __ZN8CoolPoly7PointInEss   # CoolPoly::PointIn(short, short)
  __ZN8CoolPolyC1EsPKs   # CoolPoly::CoolPoly(short, short const*)
  __ZN8CoolPolyD1Ev   # CoolPoly::~CoolPoly()
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
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
)
