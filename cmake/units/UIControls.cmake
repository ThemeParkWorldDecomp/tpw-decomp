# Unit w31b: the control factory, the controls' constructors and the button. The window's constructor is real (it builds the test
# trees); the message poster, the timers, the scroll bar, the untranslated virtual members and the draw objects are fakes in the
# unit file, which includes calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_UIControls src/game/ui/InterfaceBar.cpp src/game/ui/InterfaceButton.cpp src/game/ui/InterfaceEditBox.cpp
    src/game/ui/InterfaceImage.cpp src/game/ui/InterfaceKnob.cpp src/game/ui/InterfaceLightPanel.cpp
    src/game/ui/InterfaceListBox.cpp src/game/ui/InterfaceListControl2.cpp src/game/ui/InterfaceTextBox.cpp
    src/game/ui/InterfaceVaryBox.cpp src/game/ui/InterfaceWindow.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIControls_KEEP
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN12InterfaceBarC1EP15InterfaceWindowiissss   # InterfaceBar::InterfaceBar(InterfaceWindow*, int, int, short, short, short, short)
  __ZN13InterfaceKnobC1EP15InterfaceWindowiissss   # InterfaceKnob::InterfaceKnob(InterfaceWindow*, int, int, short, short, short, short)
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
  __ZN15InterfaceWindow12CreateObjectEiiissss   # InterfaceWindow::CreateObject(int, int, int, short, short, short, short)
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindowC1EPS_iissss   # InterfaceWindow::InterfaceWindow(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceEditBoxC1EP15InterfaceWindowiissss   # InterfaceEditBox::InterfaceEditBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceListBoxC1EP15InterfaceWindowiissss   # InterfaceListBox::InterfaceListBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceTextBoxC1EP15InterfaceWindowiissss   # InterfaceTextBox::InterfaceTextBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN16InterfaceVaryBoxC1EP15InterfaceWindowiissss   # InterfaceVaryBox::InterfaceVaryBox(InterfaceWindow*, int, int, short, short, short, short)
  __ZN19InterfaceLightPanelC1EP15InterfaceWindowiissss   # InterfaceLightPanel::InterfaceLightPanel(InterfaceWindow*, int, int, short, short, short, short)
  __ZN21InterfaceCheckButtonsC1EP15InterfaceWindowiissss   # InterfaceCheckButtons::InterfaceCheckButtons(InterfaceWindow*, int, int, short, short, short, short)
  __ZN21InterfaceListControl2C1EP15InterfaceWindowiissss   # InterfaceListControl2::InterfaceListControl2(InterfaceWindow*, int, int, short, short, short, short)
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
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_UIControls_OWN_CALLLOG ON)
