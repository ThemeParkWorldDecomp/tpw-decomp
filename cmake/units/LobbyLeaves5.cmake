# The lobby's small leaves part 5 (unit w64): HidePauseWindow, the string heap's DeAllocate, the end of the UI system (UI_CloseSystem,
# UI_CloseTimerSystem and the two queues' destructors). The timer's destructor is real (src/game/ui/InterfaceTimer.cpp) and so are the rectangles the
# window's constructor needs (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp); the window's members, the send and keyboard-capture calls, the draw object library and the
# cursor library are fakes in the unit file.
set(TPW_UNIT_LobbyLeaves5 src/game/ui/ui_mainpanel2.cpp src/game/ui/InterfaceMessageQueue.cpp
    src/game/ui/InterfaceTimer.cpp src/game/ui/UI_Base.cpp src/game/rides/RidePlacement.cpp
    src/game/ui/InterfaceFont.cpp src/game/ui/InterfaceStringHeap.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp)
set(TPW_UNIT_LobbyLeaves5_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UI_CloseSystemv   # UI_CloseSystem()
  __Z14UI_RemoveTimerP15InterfaceWindowi   # UI_RemoveTimer(InterfaceWindow*, int)
  __Z15HidePauseWindowv   # HidePauseWindow()
  __Z18SEC_AuthorizedUserv   # SEC_AuthorizedUser()
  __Z19UI_CloseTimerSystemv   # UI_CloseTimerSystem()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20UI_RemoveTimerResultP15InterfaceWindowi   # UI_RemoveTimerResult(InterfaceWindow*, int)
  __Z26SetThemeTextureDirectoriesi   # SetThemeTextureDirectories(int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN13InterfaceFont15Gen8x8DebugFontEsssss   # InterfaceFont::Gen8x8DebugFont(short, short, short, short, short)
  __ZN14InterfaceTimer9FindTimerEP15InterfaceWindowi   # InterfaceTimer::FindTimer(InterfaceWindow*, int)
  __ZN14InterfaceTimerD1Ev   # InterfaceTimer::~InterfaceTimer()
  __ZN14InterfaceTimerdlEPv   # InterfaceTimer::operator delete(void*)
  __ZN19InterfaceStringHeap10DeAllocateEv   # InterfaceStringHeap::DeAllocate()
  __ZN19InterfaceStringHeap13ReleaseStringEPDs   # InterfaceStringHeap::ReleaseString(char16_t*)
  __ZN19InterfaceStringHeap14AllocateResultEiiii   # InterfaceStringHeap::AllocateResult(int, int, int, int)
  __ZN19InterfaceStringHeap8AllocateEiiii   # InterfaceStringHeap::Allocate(int, int, int, int)
  __ZN21InterfaceMessageQueueD1Ev   # InterfaceMessageQueue::~InterfaceMessageQueue()
  __ZN22InterfaceTimedMessagesD1Ev   # InterfaceTimedMessages::~InterfaceTimedMessages()
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
  _gPauseWindow
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
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
