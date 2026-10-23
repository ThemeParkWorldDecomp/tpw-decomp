# highestZ and InterfaceListControl2::GetZOfEntries (unit w50): the window depth query. The window constructor, GetPosition and the
# rectangles under them are real; the window's other members and the list control's constructor are fakes in the unit file.
set(TPW_UNIT_UIHighestZ src/game/ui/UI_Base.cpp src/game/ui/CoolRegion.cpp src/game/ui/InterfaceKnob.cpp
    src/game/ui/InterfaceListBox.cpp src/game/ui/InterfaceListControl2.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceTimer.cpp src/game/ui/CoolRect.cpp)
set(TPW_UNIT_UIHighestZ_KEEP
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z24UI_DefaultWindowCallbackP15InterfaceWindowiii   # UI_DefaultWindowCallback(InterfaceWindow*, int, int, int)
  __Z8highestZP15InterfaceWindowP8CoolRect   # highestZ(InterfaceWindow*, CoolRect*)
  __ZN10CoolCircleC1Esss   # CoolCircle::CoolCircle(short, short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11CoolEllipseC1Essss   # CoolEllipse::CoolEllipse(short, short, short, short)
  __ZN13InterfaceKnob15SetMinMaxAnglesEPiS0_   # InterfaceKnob::SetMinMaxAngles(int*, int*)
  __ZN14InterfaceTimer4StopEv   # InterfaceTimer::Stop()
  __ZN15InterfaceWindow10BringToTopEv   # InterfaceWindow::BringToTop()
  __ZN15InterfaceWindow10ClientDrawEv   # InterfaceWindow::ClientDraw()
  __ZN15InterfaceWindow11GetPositionEP8CoolRects   # InterfaceWindow::GetPosition(CoolRect*, short)
  __ZN15InterfaceWindow11HandleScaleEssss   # InterfaceWindow::HandleScale(short, short, short, short)
  __ZN15InterfaceWindow11SetPositionEP8CoolRect   # InterfaceWindow::SetPosition(CoolRect*)
  __ZN15InterfaceWindow11doAnimFrameEv   # InterfaceWindow::doAnimFrame()
  __ZN15InterfaceWindow13ShiftPositionEss   # InterfaceWindow::ShiftPosition(short, short)
  __ZN15InterfaceWindow16DrawReverseOrderEv   # InterfaceWindow::DrawReverseOrder()
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow19GetOriginalPositionEP8CoolRect   # InterfaceWindow::GetOriginalPosition(CoolRect*)
  __ZN15InterfaceWindow6DrawMeEv   # InterfaceWindow::DrawMe()
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindow7setZRotEs   # InterfaceWindow::setZRot(short)
  __ZN15InterfaceWindow8SetValueEi   # InterfaceWindow::SetValue(int)
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
  _gUIAutoRedraw
  _gUIButtons
  _gUIDoubleClickTime
  _gUIHelpTextResult
  _gUIHighPresets
  _gUIHighestZ
  _gUIHighestZAttr
  _gUIHighestZChildZ
  _gUILastWindow
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __ZN12_GLOBAL__N_111gWideShiftsE   # gWideShifts (static object)
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
  __Z12UI_TallShiftPK15InterfaceWindow   # UI_TallShift(InterfaceWindow const*)
  __Z15UI_TallShiftForPK15InterfaceWindowb   # UI_TallShiftFor(InterfaceWindow const*, bool)
)
