# The window script interpreter and the control setters (unit w31a). The window's constructor and the rectangles are real; the
# factory, the draw objects, the senders and the region classes' other members are fakes in the unit file.
set(TPW_UNIT_UIScript src/game/ui/InterfaceWindow.cpp src/game/ui/CoolRegion.cpp src/game/ui/InterfaceKnob.cpp
    src/game/ui/InterfaceListBox.cpp src/game/ui/InterfaceListControl2.cpp src/game/ui/CoolRect.cpp
    src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIScript_KEEP
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolCircleC1Esss   # CoolCircle::CoolCircle(short, short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11CoolEllipseC1Essss   # CoolEllipse::CoolEllipse(short, short, short, short)
  __ZN13InterfaceKnob15SetMinMaxAnglesEPiS0_   # InterfaceKnob::SetMinMaxAngles(int*, int*)
  __ZN15InterfaceWindow13GetVisibilityEv   # InterfaceWindow::GetVisibility()
  __ZN15InterfaceWindow13ProcessScriptEPs   # InterfaceWindow::ProcessScript(short*)
  __ZN15InterfaceWindow13SetDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow19SetCallbackFunctionEPFiPS_iiiE   # InterfaceWindow::SetCallbackFunction(int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow19SetClientDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetClientDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow5BuildEPsPFiPS_iiiE   # InterfaceWindow::Build(short*, int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindow9CaptureKbEi   # InterfaceWindow::CaptureKb(int)
  __ZN15InterfaceWindowC1EPS_iissss   # InterfaceWindow::InterfaceWindow(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceListBox11SetListRectEssss   # InterfaceListBox::SetListRect(short, short, short, short)
  __ZN21InterfaceListControl210SetColumnsEs   # InterfaceListControl2::SetColumns(short)
  __ZN21InterfaceListControl211SetListRectEssss   # InterfaceListControl2::SetListRect(short, short, short, short)
  __ZN21InterfaceListControl212SetColumnPosEsss   # InterfaceListControl2::SetColumnPos(short, short, short)
  __ZN21InterfaceListControl213GetZOfEntriesEv   # InterfaceListControl2::GetZOfEntries()
  __ZN8CoolPolyC1EsPKs   # CoolPoly::CoolPoly(short, short const*)
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
  _gUILastWindow
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIScriptPosition
  _gUIUnitWidth
  _gUIWideAspect
  __ZN12_GLOBAL__N_111gWideShiftsE   # gWideShifts (static object)
)
