# The login panel (unit w39): CreateLoginPanel, FillLoginNames, the window's font setters and the key tables' disabling, under the real script
# interpreter, control factory, constructors and WhatAmI, so that the panel's windows are built here the way the game builds them. The
# user information, the advisor, the draw objects, the font table and the string table are fakes in the unit file, which includes
# calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_LoginPanel src/game/ui/ui_main.cpp src/game/ui/ui_playerLogin.cpp src/game/ui/ui_utils.cpp
    src/game/ui/InterfaceWindow.cpp src/game/keyboard.cpp src/game/ui/InterfaceBar.cpp src/game/ui/InterfaceButton.cpp
    src/game/ui/InterfaceEditBox.cpp src/game/ui/InterfaceImage.cpp src/game/ui/InterfaceKnob.cpp
    src/game/ui/InterfaceLightPanel.cpp src/game/ui/InterfaceListBox.cpp src/game/ui/InterfaceListControl2.cpp
    src/game/ui/InterfaceTextBox.cpp src/game/ui/InterfaceVaryBox.cpp src/game/ui/CoolRegion.cpp
    src/game/ui/CoolRect.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_LoginPanel_KEEP
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14FillLoginNamesv   # FillLoginNames()
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_TITLE_COLOURP15InterfaceWindow   # UI_TITLE_COLOUR(InterfaceWindow*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z16CreateLoginPanelv   # CreateLoginPanel()
  __Z16UIMain_GetStringi   # UIMain_GetString(int)
  __Z19DisableAllKeytablesv   # DisableAllKeytables()
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
  __ZN12InterfaceBar7WhatAmIEv   # InterfaceBar::WhatAmI()
  __ZN12InterfaceBarC1EP15InterfaceWindowiissss   # InterfaceBar::InterfaceBar(InterfaceWindow*, int, int, short, short, short, short)
  __ZN13InterfaceKnob15SetMinMaxAnglesEPiS0_   # InterfaceKnob::SetMinMaxAngles(int*, int*)
  __ZN13InterfaceKnob7WhatAmIEv   # InterfaceKnob::WhatAmI()
  __ZN13InterfaceKnobC1EP15InterfaceWindowiissss   # InterfaceKnob::InterfaceKnob(InterfaceWindow*, int, int, short, short, short, short)
  __ZN14InterfaceImage7WhatAmIEv   # InterfaceImage::WhatAmI()
  __ZN14InterfaceImageC1EP15InterfaceWindowiissss   # InterfaceImage::InterfaceImage(InterfaceWindow*, int, int, short, short, short, short)
  __ZN15InterfaceButton11GetHelpTextEv   # InterfaceButton::GetHelpText()
  __ZN15InterfaceButton12DoTextColourEv   # InterfaceButton::DoTextColour()
  __ZN15InterfaceButton13SetAutoRepeatEii   # InterfaceButton::SetAutoRepeat(int, int)
  __ZN15InterfaceButton13StopHeldTimerEv   # InterfaceButton::StopHeldTimer()
  __ZN15InterfaceButton15SetTextUpColourE7UI_RGBA   # InterfaceButton::SetTextUpColour(UI_RGBA)
  __ZN15InterfaceButton17SetTextDownColourE7UI_RGBA   # InterfaceButton::SetTextDownColour(UI_RGBA)
  __ZN15InterfaceButton6EnableEs   # InterfaceButton::Enable(short)
  __ZN15InterfaceButton7GetTextEv   # InterfaceButton::GetText()
  __ZN15InterfaceButton7HiLightEaa   # InterfaceButton::HiLight(signed char, signed char)
  __ZN15InterfaceButton7SetTextEPDs   # InterfaceButton::SetText(char16_t*)
  __ZN15InterfaceButton7WhatAmIEv   # InterfaceButton::WhatAmI()
  __ZN15InterfaceButton8GetStateEv   # InterfaceButton::GetState()
  __ZN15InterfaceButton8SetStateEaa   # InterfaceButton::SetState(signed char, signed char)
  __ZN15InterfaceButton9CalcFrameEv   # InterfaceButton::CalcFrame()
  __ZN15InterfaceButtonC1EP15InterfaceWindowiissss   # InterfaceButton::InterfaceButton(InterfaceWindow*, int, int, short, short, short, short)
  __ZN15InterfaceButtonD1Ev   # InterfaceButton::~InterfaceButton()
  __ZN15InterfaceWindow11SetFontJustEii   # InterfaceWindow::SetFontJust(int, int)
  __ZN15InterfaceWindow11SetFontWrapEii   # InterfaceWindow::SetFontWrap(int, int)
  __ZN15InterfaceWindow12CreateObjectEiiissss   # InterfaceWindow::CreateObject(int, int, int, short, short, short, short)
  __ZN15InterfaceWindow13GetVisibilityEv   # InterfaceWindow::GetVisibility()
  __ZN15InterfaceWindow13ProcessScriptEPs   # InterfaceWindow::ProcessScript(short*)
  __ZN15InterfaceWindow13SetDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow13SetFontColourEiiii   # InterfaceWindow::SetFontColour(int, int, int, int)
  __ZN15InterfaceWindow13getExtraParamEs   # InterfaceWindow::getExtraParam(short)
  __ZN15InterfaceWindow13setExtraParamEsPv   # InterfaceWindow::setExtraParam(short, void*)
  __ZN15InterfaceWindow14CalcClientAreaEv   # InterfaceWindow::CalcClientArea()
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow17SetClientDrawTextEPDs   # InterfaceWindow::SetClientDrawText(char16_t*)
  __ZN15InterfaceWindow19SetCallbackFunctionEPFiPS_iiiE   # InterfaceWindow::SetCallbackFunction(int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow19SetClientDrawObjectEP19InterfaceDrawObject   # InterfaceWindow::SetClientDrawObject(InterfaceDrawObject*)
  __ZN15InterfaceWindow24CallDefaultWindowHandlerEiii   # InterfaceWindow::CallDefaultWindowHandler(int, int, int)
  __ZN15InterfaceWindow5BuildEPsPFiPS_iiiE   # InterfaceWindow::Build(short*, int (*)(InterfaceWindow*, int, int, int))
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindow7SetFontEP13InterfaceFont   # InterfaceWindow::SetFont(InterfaceFont*)
  __ZN15InterfaceWindow7SetTextEPDs   # InterfaceWindow::SetText(char16_t*)
  __ZN15InterfaceWindow7WhatAmIEv   # InterfaceWindow::WhatAmI()
  __ZN15InterfaceWindow9CaptureKbEi   # InterfaceWindow::CaptureKb(int)
  __ZN15InterfaceWindowC1EPS_iissss   # InterfaceWindow::InterfaceWindow(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceEditBox7WhatAmIEv   # InterfaceEditBox::WhatAmI()
  __ZN16InterfaceEditBoxC1EP15InterfaceWindowiissss   # InterfaceEditBox::InterfaceEditBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceListBox11SetListRectEssss   # InterfaceListBox::SetListRect(short, short, short, short)
  __ZN16InterfaceListBox7WhatAmIEv   # InterfaceListBox::WhatAmI()
  __ZN16InterfaceListBoxC1EP15InterfaceWindowiissss   # InterfaceListBox::InterfaceListBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceTextBox7WhatAmIEv   # InterfaceTextBox::WhatAmI()
  __ZN16InterfaceTextBoxC1EP15InterfaceWindowiissss   # InterfaceTextBox::InterfaceTextBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceVaryBox7WhatAmIEv   # InterfaceVaryBox::WhatAmI()
  __ZN16InterfaceVaryBoxC1EP15InterfaceWindowiissss   # InterfaceVaryBox::InterfaceVaryBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN19InterfaceLightPanel7WhatAmIEv   # InterfaceLightPanel::WhatAmI()
  __ZN19InterfaceLightPanelC1EP15InterfaceWindowiissss   # InterfaceLightPanel::InterfaceLightPanel(InterfaceWindow*, int, int, short, short, short, short)
  __ZN21InterfaceCheckButtons7WhatAmIEv   # InterfaceCheckButtons::WhatAmI()
  __ZN21InterfaceCheckButtonsC1EP15InterfaceWindowiissss   # InterfaceCheckButtons::InterfaceCheckButtons(InterfaceWindow*, int, int, short, short, short, short)
  __ZN21InterfaceListControl210SetColumnsEs   # InterfaceListControl2::SetColumns(short)
  __ZN21InterfaceListControl211SetListRectEssss   # InterfaceListControl2::SetListRect(short, short, short, short)
  __ZN21InterfaceListControl212SetColumnPosEsss   # InterfaceListControl2::SetColumnPos(short, short, short)
  __ZN21InterfaceListControl213GetZOfEntriesEv   # InterfaceListControl2::GetZOfEntries()
  __ZN21InterfaceListControl27WhatAmIEv   # InterfaceListControl2::WhatAmI()
  __ZN21InterfaceListControl2C1EP15InterfaceWindowiissss   # InterfaceListControl2::InterfaceListControl2(InterfaceWindow*, int, int, short, short, short, short)
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
  _gKeyTable
  _gKeyTableCamera
  _gKeyTableCheat
  _gKeyTableCoaster
  _gKeyTableDebug
  _gKeyTableGame
  _gKeyTableShortcuts
  _gLoginNameBlue
  _gLoginNameGreen
  _gLoginNameRed
  _gLoginPanelFirstPlayer
  _gLoginPanelWindow
  _gLoginPanelWindowData
  _gUIHighPresets
  _gUILastWindow
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIScriptPosition
  _gUIUnitWidth
  _gUIWideAspect
  __ZN12_GLOBAL__N_111gWideShiftsE   # gWideShifts (static object)
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LoginPanel_OWN_CALLLOG ON)
