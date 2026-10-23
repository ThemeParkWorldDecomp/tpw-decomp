# The window's destructor chain (unit w51): ~InterfaceWindow, PropagateDestruction, UnCaptureKeys, UI_RemoveTimer, FindTimer, ClearAllPointers, the
# three UI_Invalidate*Messages and the two queues' InvalidateMsgs. The window's constructor, UI_SendMessage and the rectangles are real; the
# draw objects, the regions, the help text, the pointer update and the message poster are fakes in the unit file.
# Unit w54 adds the leftovers on the destroy path: the base window's small virtuals (InterfaceWindow_virtuals.cpp), the bar's and the text box's
# destructors (ui_controls_dtors.cpp), the timer's destructor (UI_timers_remove.cpp), the string heap's ReleaseString (lobby_start_leaves.cpp)
# and the button (InterfaceButton.cpp, for its overrides of Enable and GetHelpText and its destructor).
set(TPW_UNIT_UIDestroy src/game/ui/InterfaceWindow.cpp src/game/ui/InterfaceTimer.cpp
    src/game/ui/InterfaceMessageQueue.cpp src/game/ui/UI_Base.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp
    src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceBar.cpp src/game/ui/InterfaceTextBox.cpp
    src/game/rides/RidePlacement.cpp src/game/ui/InterfaceFont.cpp src/game/ui/InterfaceStringHeap.cpp
    src/game/ui/InterfaceButton.cpp)
set(TPW_UNIT_UIDestroy_KEEP
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UI_RemoveTimerP15InterfaceWindowi   # UI_RemoveTimer(InterfaceWindow*, int)
  __Z14UI_SendMessageP15InterfaceWindowiii   # UI_SendMessage(InterfaceWindow*, int, int, int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z16ClearAllPointersP15InterfaceWindow   # ClearAllPointers(InterfaceWindow*)
  __Z18SEC_AuthorizedUserv   # SEC_AuthorizedUser()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20UI_RemoveTimerResultP15InterfaceWindowi   # UI_RemoveTimerResult(InterfaceWindow*, int)
  __Z21UI_InvalidateMessagesP15InterfaceWindow   # UI_InvalidateMessages(InterfaceWindow*)
  __Z25UI_InvalidateDrawMessagesP15InterfaceWindow   # UI_InvalidateDrawMessages(InterfaceWindow*)
  __Z26SetThemeTextureDirectoriesi   # SetThemeTextureDirectories(int)
  __Z29UI_InvalidateDeferredMessagesP15InterfaceWindow   # UI_InvalidateDeferredMessages(InterfaceWindow*)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN12InterfaceBarD1Ev   # InterfaceBar::~InterfaceBar()
  __ZN13InterfaceFont15Gen8x8DebugFontEsssss   # InterfaceFont::Gen8x8DebugFont(short, short, short, short, short)
  __ZN14InterfaceTimer9FindTimerEP15InterfaceWindowi   # InterfaceTimer::FindTimer(InterfaceWindow*, int)
  __ZN14InterfaceTimerD1Ev   # InterfaceTimer::~InterfaceTimer()
  __ZN14InterfaceTimerdlEPv   # InterfaceTimer::operator delete(void*)
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
  __ZN15InterfaceWindow11GetHelpTextEv   # InterfaceWindow::GetHelpText()
  __ZN15InterfaceWindow12DrawFunctionEv   # InterfaceWindow::DrawFunction()
  __ZN15InterfaceWindow13UnCaptureKeysEv   # InterfaceWindow::UnCaptureKeys()
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow17CallWindowHandlerEiii   # InterfaceWindow::CallWindowHandler(int, int, int)
  __ZN15InterfaceWindow20PropagateDestructionEv   # InterfaceWindow::PropagateDestruction()
  __ZN15InterfaceWindow6EnableEs   # InterfaceWindow::Enable(short)
  __ZN15InterfaceWindow7GetTextEv   # InterfaceWindow::GetText()
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindow8GetValueEv   # InterfaceWindow::GetValue()
  __ZN15InterfaceWindowC1EPS_iissss   # InterfaceWindow::InterfaceWindow(InterfaceWindow*, int, int, short, short, short, short)
  __ZN15InterfaceWindowD1Ev   # InterfaceWindow::~InterfaceWindow()
  __ZN15InterfaceWindowmiERS_   # InterfaceWindow::operator-(InterfaceWindow&)
  __ZN16InterfaceTextBoxD1Ev   # InterfaceTextBox::~InterfaceTextBox()
  __ZN19InterfaceStringHeap10DeAllocateEv   # InterfaceStringHeap::DeAllocate()
  __ZN19InterfaceStringHeap13ReleaseStringEPDs   # InterfaceStringHeap::ReleaseString(char16_t*)
  __ZN19InterfaceStringHeap14AllocateResultEiiii   # InterfaceStringHeap::AllocateResult(int, int, int, int)
  __ZN19InterfaceStringHeap8AllocateEiiii   # InterfaceStringHeap::Allocate(int, int, int, int)
  __ZN21InterfaceMessageQueue14InvalidateMsgsEP15InterfaceWindow   # InterfaceMessageQueue::InvalidateMsgs(InterfaceWindow*)
  __ZN22InterfaceTimedMessages14InvalidateMsgsEP15InterfaceWindow   # InterfaceTimedMessages::InvalidateMsgs(InterfaceWindow*)
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
  _gAuthorizationStamp
  _gAuthorizedUser
  _gSecurityKey
  _gSecurityMask
  _gSecurityText
  _gSharedTextureFolder
  _gStringHeapRowsA
  _gStringHeapRowsB
  _gStringHeapTableA
  _gStringHeapTableB
  _gStringHeapWidthA
  _gStringHeapWidthB
  _gThemeTextureFolder
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
