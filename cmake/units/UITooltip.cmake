# The tooltip window (unit w56): CUIHelpWindow, its static object, InitHelpText, HelpTextMsg, SetHelpTextString and MoveHelpText
# (src/game/ui/ui_tooltip.cpp), with the window's GetOriginalPosition, GetPosition, SetPosition and ShiftPosition (src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp), the
# help text handler it derives from (src/game/ui/InterfaceHelpText.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp), the window's constructor and the rectangles, all real. Every other window member,
# the font table, the draw objects, the string table and the message senders are fakes in the unit file.
set(TPW_UNIT_UITooltip src/game/ui/ui_tooltip.cpp src/game/ui/InterfaceHelpText.cpp src/game/ui/InterfaceTimer.cpp
    src/game/ui/UI_Base.cpp src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp)
set(TPW_UNIT_UITooltip_KEEP
  __Z11HelpTextMsgP15InterfaceWindowiii   # HelpTextMsg(InterfaceWindow*, int, int, int)
  __Z12InitHelpTextv   # InitHelpText()
  __Z12MoveHelpTextss   # MoveHelpText(short, short)
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z17SetHelpTextStringPDs   # SetHelpTextString(char16_t*)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21UI_SetHelpTextHandlerP17InterfaceHelpText   # UI_SetHelpTextHandler(InterfaceHelpText*)
  __Z24UI_DefaultWindowCallbackP15InterfaceWindowiii   # UI_DefaultWindowCallback(InterfaceWindow*, int, int, int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN13CUIHelpWindow12HideHelpTextEv   # CUIHelpWindow::HideHelpText()
  __ZN13CUIHelpWindow12ShowHelpTextEv   # CUIHelpWindow::ShowHelpText()
  __ZN13CUIHelpWindow14OnMouseMovedInEP15InterfaceWindow   # CUIHelpWindow::OnMouseMovedIn(InterfaceWindow*)
  __ZN13CUIHelpWindow4InitEv   # CUIHelpWindow::Init()
  __ZN13CUIHelpWindow6DeInitEv   # CUIHelpWindow::DeInit()
  __ZN13CUIHelpWindow6EnableEv   # CUIHelpWindow::Enable()
  __ZN13CUIHelpWindow6UpdateEv   # CUIHelpWindow::Update()
  __ZN13CUIHelpWindow7DisableEv   # CUIHelpWindow::Disable()
  __ZN13CUIHelpWindow7OnFrameEv   # CUIHelpWindow::OnFrame()
  __ZN13CUIHelpWindowD1Ev   # CUIHelpWindow::~CUIHelpWindow()
  __ZN14InterfaceTimer4StopEv   # InterfaceTimer::Stop()
  __ZN14InterfaceTimerC1Ev   # InterfaceTimer::InterfaceTimer()
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
  __ZN17InterfaceHelpText11OnMouseDownEv   # InterfaceHelpText::OnMouseDown()
  __ZN17InterfaceHelpText12HideHelpTextEv   # InterfaceHelpText::HideHelpText()
  __ZN17InterfaceHelpText12OnMouseMovedEss   # InterfaceHelpText::OnMouseMoved(short, short)
  __ZN17InterfaceHelpText12ShowHelpTextEv   # InterfaceHelpText::ShowHelpText()
  __ZN17InterfaceHelpText14OnHideOrDeleteEP15InterfaceWindow   # InterfaceHelpText::OnHideOrDelete(InterfaceWindow*)
  __ZN17InterfaceHelpText14OnMouseMovedInEP15InterfaceWindow   # InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*)
  __ZN17InterfaceHelpText15OnMouseMovedOutEv   # InterfaceHelpText::OnMouseMovedOut()
  __ZN17InterfaceHelpText4HideEv   # InterfaceHelpText::Hide()
  __ZN17InterfaceHelpText4InitEv   # InterfaceHelpText::Init()
  __ZN17InterfaceHelpText4ShowEv   # InterfaceHelpText::Show()
  __ZN17InterfaceHelpText6DeInitEv   # InterfaceHelpText::DeInit()
  __ZN17InterfaceHelpText6EnableEv   # InterfaceHelpText::Enable()
  __ZN17InterfaceHelpText6UpdateEv   # InterfaceHelpText::Update()
  __ZN17InterfaceHelpText7DisableEv   # InterfaceHelpText::Disable()
  __ZN17InterfaceHelpText7OnFrameEv   # InterfaceHelpText::OnFrame()
  __ZN17InterfaceHelpTextC1Ev   # InterfaceHelpText::InterfaceHelpText()
  __ZN17InterfaceHelpTextD1Ev   # InterfaceHelpText::~InterfaceHelpText()
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
  _gUIHelpTextString
  _gUIHelpWindow
  _gUIHighPresets
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
