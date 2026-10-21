# The interface's message pump (unit w29): the dispatch, the timers, TickUIErrors; the queues' Add and the window constructor are real.
set(TPW_UNIT_UIPump src/game/ui/InterfaceMessageQueue.cpp src/game/ui/InterfaceTimer.cpp
    src/game/ui/InterfaceWindow.cpp src/game/ui/UIInterface.cpp src/game/ui/UI_Base.cpp src/game/ui/ui_ErrorDisplay.cpp
    src/game/ui/ui_Postcard.cpp src/game/ui/ui_main.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp
    src/game/ui/InterfaceCursor.cpp src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceDrawObject.cpp
    platform/arena.cpp)
set(TPW_UNIT_UIPump_KEEP
  __Z10UI_KeyCharsi   # UI_KeyChar(short, int)
  __Z10UI_Keydownsi   # UI_Keydown(short, int)
  __Z11IveGotTicksv   # IveGotTicks()
  __Z11UI_Activatev   # UI_Activate()
  __Z11UI_AddTimerP15InterfaceWindowii   # UI_AddTimer(InterfaceWindow*, int, int)
  __Z12TickUIErrorsv   # TickUIErrors()
  __Z12UI_SetCursors   # UI_SetCursor(short)
  __Z12UI_WideShiftPK15InterfaceWindow   # UI_WideShift(InterfaceWindow const*)
  __Z13UI_DeActivatev   # UI_DeActivate()
  __Z13UI_HideCursorv   # UI_HideCursor()
  __Z13UI_InitSystemiib   # UI_InitSystem(int, int, bool)
  __Z13UI_ShowCursorv   # UI_ShowCursor()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UI_GetHighestZP15InterfaceWindowP8CoolRect   # UI_GetHighestZ(InterfaceWindow*, CoolRect*)
  __Z14UI_PostMessageP15InterfaceWindowiii   # UI_PostMessage(InterfaceWindow*, int, int, int)
  __Z14UI_SendMessageP15InterfaceWindowiii   # UI_SendMessage(InterfaceWindow*, int, int, int)
  __Z15UI_IsWideWindowPK15InterfaceWindow   # UI_IsWideWindow(InterfaceWindow const*)
  __Z15UI_WideShiftForPK15InterfaceWindowb   # UI_WideShiftFor(InterfaceWindow const*, bool)
  __Z17UI_DeviceButtonUpsss   # UI_DeviceButtonUp(short, short, short)
  __Z17UI_EnableHelpTextv   # UI_EnableHelpText()
  __Z17UI_GetButtonFlagsv   # UI_GetButtonFlags()
  __Z17UI_UseCursorTableP11CursorTable   # UI_UseCursorTable(CursorTable*)
  __Z18UI_DisableHelpTextv   # UI_DisableHelpText()
  __Z18UI_InitTimerSystemi   # UI_InitTimerSystem(int)
  __Z18UI_PostcardPendingv   # UI_PostcardPending()
  __Z18UI_SetPeekCallBackPFiP15InterfaceWindowiiiE   # UI_SetPeekCallBack(int (*)(InterfaceWindow*, int, int, int))
  __Z18UI_UpdatePointerInv   # UI_UpdatePointerIn()
  __Z19UI_DeviceButtonDownsss   # UI_DeviceButtonDown(short, short, short)
  __Z19UI_DeviceMouseWheelisss   # UI_DeviceMouseWheel(int, short, short, short)
  __Z19UI_DispatchMessagesi   # UI_DispatchMessages(int)
  __Z19UI_FlushAllMessagesv   # UI_FlushAllMessages()
  __Z19UI_ServiceAllTimersv   # UI_ServiceAllTimers()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20UI_DevicePointerMovess   # UI_DevicePointerMove(short, short)
  __Z20UI_SetStandardCursort   # UI_SetStandardCursor(unsigned short)
  __Z22UI_SendOnFrameMessagesv   # UI_SendOnFrameMessages()
  __Z23UI_DispatchDrawMessagesi   # UI_DispatchDrawMessages(int)
  __Z23UI_SetClockTickFunctionP17clockTickFunction   # UI_SetClockTickFunction(clockTickFunction*)
  __Z27UI_DispatchDeferredMessagesv   # UI_DispatchDeferredMessages()
  __Z7onframeP15InterfaceWindow   # onframe(InterfaceWindow*)
  __Z8UI_Keyupsi   # UI_Keyup(short, int)
  __Z9ProcessUIv   # ProcessUI()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11CursorTable10DeAllocateEv   # CursorTable::DeAllocate()
  __ZN11CursorTable8AllocateEt   # CursorTable::Allocate(unsigned short)
  __ZN11CursorTable8GetEntryEt   # CursorTable::GetEntry(unsigned short)
  __ZN11CursorTable8SetEntryEtP15InterfaceCursor   # CursorTable::SetEntry(unsigned short, InterfaceCursor*)
  __ZN11CursorTableC1Ev   # CursorTable::CursorTable()
  __ZN11CursorTableD1Ev   # CursorTable::~CursorTable()
  __ZN11UIInterface7ProcessEv   # UIInterface::Process()
  __ZN11UIInterface7UnPauseEv   # UIInterface::UnPause()
  __ZN15InterfaceCursor10GetVisibleEv   # InterfaceCursor::GetVisible()
  __ZN15InterfaceCursor3UseEs   # InterfaceCursor::Use(short)
  __ZN15InterfaceCursor4DrawEss   # InterfaceCursor::Draw(short, short)
  __ZN15InterfaceCursor4HideEv   # InterfaceCursor::Hide()
  __ZN15InterfaceCursor4ShowEv   # InterfaceCursor::Show()
  __ZN15InterfaceCursor6SetHotEcc   # InterfaceCursor::SetHot(char, char)
  __ZN15InterfaceCursor7AnimateEi   # InterfaceCursor::Animate(int)
  __ZN15InterfaceCursor7SetDataEPvc   # InterfaceCursor::SetData(void*, char)
  __ZN15InterfaceCursorC1Ev   # InterfaceCursor::InterfaceCursor()
  __ZN15InterfaceCursorD1Ev   # InterfaceCursor::~InterfaceCursor()
  __ZN15InterfaceWindow16MakeRectAbsoluteEP8CoolRect   # InterfaceWindow::MakeRectAbsolute(CoolRect*)
  __ZN15InterfaceWindow17CallWindowHandlerEiii   # InterfaceWindow::CallWindowHandler(int, int, int)
  __ZN15InterfaceWindow19AmIGettingDestroyedEv   # InterfaceWindow::AmIGettingDestroyed()
  __ZN15InterfaceWindow7GetAttrEv   # InterfaceWindow::GetAttr()
  __ZN15InterfaceWindow7GetNextEv   # InterfaceWindow::GetNext()
  __ZN15InterfaceWindow7PointInEss   # InterfaceWindow::PointIn(short, short)
  __ZN15InterfaceWindow8GetChildEi   # InterfaceWindow::GetChild(int)
  __ZN15InterfaceWindowC1EPS_iissss   # InterfaceWindow::InterfaceWindow(InterfaceWindow*, int, int, short, short, short, short)
  __ZN17DrawObjectLibrary13AddDrawObjectEP19InterfaceDrawObjectPc   # DrawObjectLibrary::AddDrawObject(InterfaceDrawObject*, char*)
  __ZN17DrawObjectLibrary14FindDrawObjectEPc   # DrawObjectLibrary::FindDrawObject(char*)
  __ZN17DrawObjectLibrary14FindDrawObjectEj   # DrawObjectLibrary::FindDrawObject(unsigned int)
  __ZN17DrawObjectLibrary9RemoveAllEs   # DrawObjectLibrary::RemoveAll(short)
  __ZN19InterfaceDrawObject10InstanceMeEv   # InterfaceDrawObject::InstanceMe()
  __ZN19InterfaceDrawObject12SetFrameBaseEs   # InterfaceDrawObject::SetFrameBase(short)
  __ZN19InterfaceDrawObject19GetWritableDataAreaEPiS0_   # InterfaceDrawObject::GetWritableDataArea(int*, int*)
  __ZN19InterfaceDrawObject4DrawEsP8CoolRects   # InterfaceDrawObject::Draw(short, CoolRect*, short)
  __ZN19InterfaceDrawObject4DrawEsP8CoolRectsPDs   # InterfaceDrawObject::Draw(short, CoolRect*, short, char16_t*)
  __ZN19InterfaceDrawObject4ShowEs   # InterfaceDrawObject::Show(short)
  __ZN19InterfaceDrawObject5QueryEjl   # InterfaceDrawObject::Query(unsigned int, long)
  __ZN19InterfaceDrawObject6ChangeEjP8DOChange   # InterfaceDrawObject::Change(unsigned int, DOChange*)
  __ZN19InterfaceDrawObject7RefreshEv   # InterfaceDrawObject::Refresh()
  __ZN19InterfaceDrawObject8SetFrameEs   # InterfaceDrawObject::SetFrame(short)
  __ZN19InterfaceDrawObjectC1EPv   # InterfaceDrawObject::InterfaceDrawObject(void*)
  __ZN19InterfaceDrawObjectD1Ev   # InterfaceDrawObject::~InterfaceDrawObject()
  __ZN21InterfaceMessageQueue10InitialiseEi   # InterfaceMessageQueue::Initialise(int)
  __ZN21InterfaceMessageQueue3AddEP15InterfaceWindowiii   # InterfaceMessageQueue::Add(InterfaceWindow*, int, int, int)
  __ZN21InterfaceMessageQueue4PeekEv   # InterfaceMessageQueue::Peek()
  __ZN21InterfaceMessageQueue4PostEv   # InterfaceMessageQueue::Post()
  __ZN21InterfaceMessageQueue5FlushEv   # InterfaceMessageQueue::Flush()
  __ZN21InterfaceMessageQueue8GetNumQdEv   # InterfaceMessageQueue::GetNumQd()
  __ZN21InterfaceMessageQueueC1Ev   # InterfaceMessageQueue::InterfaceMessageQueue()
  __ZN22InterfaceTimedMessages10InitialiseEi   # InterfaceTimedMessages::Initialise(int)
  __ZN22InterfaceTimedMessages5FlushEv   # InterfaceTimedMessages::Flush()
  __ZN22InterfaceTimedMessages7ProcessEv   # InterfaceTimedMessages::Process()
  __ZN22InterfaceTimedMessagesC1Ev   # InterfaceTimedMessages::InterfaceTimedMessages()
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRects   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short)
  __ZN23InterfaceDrawEngineMesh4DrawEsP8CoolRectsPDs   # InterfaceDrawEngineMesh::Draw(short, CoolRect*, short, char16_t*)
  __ZN23InterfaceDrawEngineMesh4ShowEs   # InterfaceDrawEngineMesh::Show(short)
  __ZN23InterfaceDrawEngineMeshC1EPv   # InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void*)
  __ZN23InterfaceDrawEngineMeshD1Ev   # InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh()
  __ZN33InterfaceDrawEngineResizableFrameC1EPv   # InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void*)
  __ZN33InterfaceDrawEngineResizableFrameD1Ev   # InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame()
  __ZN34InterfaceDrawEngineResizableSliderC1EPv   # InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void*)
  __ZN34InterfaceDrawEngineResizableSliderD1Ev   # InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider()
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
  _gDrawObjectLibrary
  _gPostcardDelay
  _gPostcardWanted
  _gUIActive
  _gUIButtonFlags
  _gUIButtonWindow
  _gUICaptureDevice
  _gUIClock
  _gUICursor
  _gUICursorNumber
  _gUICursorTable
  _gUIDefaultClock
  _gUIDefaultHelpText
  _gUIDrawQueue
  _gUIDrawQueued
  _gUIErrorDialog
  _gUIErrorMutex
  _gUIErrorQueue
  _gUIFocusWindow
  _gUIGeneralQueued
  _gUIHeap
  _gUIHebrew
  _gUIHelpText
  _gUIHighPresets
  _gUIKeyCapture
  _gUIKeyboardWindow
  _gUILastWindow
  _gUIMessageQueue
  _gUIOnFrameCount
  _gUIPeekCallBack
  _gUIPointerIn
  _gUIPointerX
  _gUIPointerY
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIRootWindow
  _gUIStandardCursor
  _gUIStarted
  _gUITimedMessages
  _gUITimerCount
  _gUITimerList
  _gUITimers
  _gUITimersPaused
  _gUIUnitWidth
  _gUIWideAspect
  __ZN12_GLOBAL__N_111gWideShiftsE   # gWideShifts (static object)
)
