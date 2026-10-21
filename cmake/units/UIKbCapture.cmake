# The keyboard decision (unit w49): UIMain_DecideKbCapture, MainMenuUp, UIMenu::IsShown and the coaster panel's two small functions,
# with the real key table Enable and the window's CaptureKb (the capture global and the two posted messages). The message poster, the
# window script's callees that CaptureKb's file needs to link, and GetAttr (one line of the message pump's file) are in the unit file.
set(TPW_UNIT_UIKbCapture src/game/ui/UIMenu.cpp src/game/ui/ui_main.cpp src/game/ui/ui_mainmenu.cpp
    src/game/ui/ui_coasterPanel.cpp src/game/keyboard.cpp src/game/ui/InterfaceWindow.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIKbCapture_KEEP
  __Z10MainMenuUpv   # MainMenuUp()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z18CoasterEditPanelUpv   # CoasterEditPanelUp()
  __Z18EnableAllKeytablesv   # EnableAllKeytables()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21CoasterEditPanelGetKbv   # CoasterEditPanelGetKb()
  __Z22UIMain_DecideKbCapturev   # UIMain_DecideKbCapture()
  __ZN10CoolCircle5ShiftEss   # CoolCircle::Shift(short, short)
  __ZN10CoolCircle6AdjustEssss   # CoolCircle::Adjust(short, short, short, short)
  __ZN10CoolCircle7PointInEss   # CoolCircle::PointIn(short, short)
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
  __ZN11CoolEllipseD1Ev   # CoolEllipse::~CoolEllipse()
  __ZN15InterfaceWindow13GetVisibilityEv   # InterfaceWindow::GetVisibility()
  __ZN15InterfaceWindow13ProcessScriptEPs   # InterfaceWindow::ProcessScript(short*)
  __ZN15InterfaceWindow13SetDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow19SetCallbackFunctionEPFiPS_iiiE   # InterfaceWindow::SetCallbackFunction(int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow19SetClientDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetClientDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow5BuildEPsPFiPS_iiiE   # InterfaceWindow::Build(short*, int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow9CaptureKbEi   # InterfaceWindow::CaptureKb(int)
  __ZN6UIMenu7IsShownEv   # UIMenu::IsShown()
  __ZN8CoolPoly5ShiftEss   # CoolPoly::Shift(short, short)
  __ZN8CoolPoly6AdjustEssss   # CoolPoly::Adjust(short, short, short, short)
  __ZN8CoolPoly7PointInEss   # CoolPoly::PointIn(short, short)
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
  __ZN9CKeyTable6EnableEv   # CKeyTable::Enable()
  __ZN9CKeyTable6SearchEss   # CKeyTable::Search(short, short)
  __ZN9CKeyTable7DisableEv   # CKeyTable::Disable()
  _gBankruptWindow
  _gBumholeWindow
  _gCoasterEditMode
  _gCoasterPanel
  _gFirstPersonWindow
  _gGameWindow
  _gMainMenu
  _gOnlinePanel
  _gOnlineWindow
  _gUIHighPresets
  _gUIPanelWindow
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIScriptPosition
  _gUIUnitWidth
  _gUIWideAspect
)
