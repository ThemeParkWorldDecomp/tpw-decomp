# Unit w55: peek filter, highlight, IME and UIInterface::UnPause.
# Every boundary records; windows have native kinds and the Mac fixtures have real vtables.
set(TPW_UNIT_UIPeek src/game/ui/ui_main.cpp src/game/HLSprite.cpp src/game/ui/ui_IME.cpp
    src/game/ui/InterfaceMessageQueue.cpp src/game/ui/InterfaceTimer.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/UIInterface.cpp src/game/ui/UI_Base.cpp src/game/ui/ui_ErrorDisplay.cpp src/game/ui/ui_Postcard.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp)
set(TPW_UNIT_UIPeek_KEEP
  __Z11IveGotTicksv   # IveGotTicks()
  __Z11UI_AddTimerP15InterfaceWindowii   # UI_AddTimer(InterfaceWindow*, int, int)
  __Z12TickUIErrorsv   # TickUIErrors()
  __Z13EffectPeekMsgP15InterfaceWindowiii   # EffectPeekMsg(InterfaceWindow*, int, int, int)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UI_GetHighestZP15InterfaceWindowP8CoolRect   # UI_GetHighestZ(InterfaceWindow*, CoolRect*)
  __Z16BTN_HighLight_Onffff   # BTN_HighLight_On(float, float, float, float)
  __Z17BTN_HighLight_Offv   # BTN_HighLight_Off()
  __Z18BTN_HighLight_Killv   # BTN_HighLight_Kill()
  __Z18BTN_HighLight_Moveff   # BTN_HighLight_Move(float, float)
  __Z18UI_PostcardPendingv   # UI_PostcardPending()
  __Z19UI_DispatchMessagesi   # UI_DispatchMessages(int)
  __Z19UI_ServiceAllTimersv   # UI_ServiceAllTimers()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z22UI_SendOnFrameMessagesv   # UI_SendOnFrameMessages()
  __Z23UI_DispatchDrawMessagesi   # UI_DispatchDrawMessages(int)
  __Z27UI_DispatchDeferredMessagesv   # UI_DispatchDeferredMessages()
  __Z7onframeP15InterfaceWindow   # onframe(InterfaceWindow*)
  __Z9ProcessUIv   # ProcessUI()
  __Z9UnPauseUIv   # UnPauseUI()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11UIInterface7ProcessEv   # UIInterface::Process()
  __ZN11UIInterface7UnPauseEv   # UIInterface::UnPause()
  __ZN15InterfaceWindow19AmIGettingDestroyedEv   # InterfaceWindow::AmIGettingDestroyed()
  __ZN15InterfaceWindow7GetAttrEv   # InterfaceWindow::GetAttr()
  __ZN15InterfaceWindow7GetNextEv   # InterfaceWindow::GetNext()
  __ZN15InterfaceWindow8GetChildEi   # InterfaceWindow::GetChild(int)
  __ZN21InterfaceMessageQueue4PeekEv   # InterfaceMessageQueue::Peek()
  __ZN21InterfaceMessageQueue4PostEv   # InterfaceMessageQueue::Post()
  __ZN21InterfaceMessageQueue8GetNumQdEv   # InterfaceMessageQueue::GetNumQd()
  __ZN22InterfaceTimedMessages7ProcessEv   # InterfaceTimedMessages::Process()
  __ZN6CUIIME10DeActivateEv   # CUIIME::DeActivate()
  __ZN6CUIIME22InvalidateStoredStringEP15InterfaceWindow   # CUIIME::InvalidateStoredString(InterfaceWindow*)
  __ZN6CUIIME8ActivateEP15InterfaceWindow   # CUIIME::Activate(InterfaceWindow*)
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
  _gHLAreaScale
  _gHLAreaScaleSet
  _gHLBaseArea
  _gHLBaseAreaSet
  _gHLMaxEmitters
  _gHLMinEmitters
  _gPostcardDelay
  _gPostcardWanted
  _gUIDrawQueued
  _gUIErrorDialog
  _gUIErrorMutex
  _gUIErrorQueue
  _gUIFocusWindow
  _gUIGeneralQueued
  _gUIHighPresets
  _gUIIME
  _gUIKeyboardWindow
  _gUIOnFrameCount
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
