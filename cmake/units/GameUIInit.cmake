# Park interface set-up and teardown (w116), with real rectangles and all external callees faked.
set(TPW_UNIT_GameUIInit src/game/Mouse.cpp src/game/ui/UIInterface.cpp src/game/ui/ui_main.cpp src/game/ui/ui_tags.cpp
    src/game/ui/ui_cashPanel.cpp src/game/ui/ui_coasterPanel.cpp src/game/ui/ui_controlPanel.cpp
    src/game/ui/ui_staffPanel.cpp src/game/ui/ui_allKidsPanel.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/ui_StatusPanel.cpp src/game/ui/ui_entryPricePanel.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_GameUIInit_KEEP
  __Z10InitGameUIv   # InitGameUI()
  __Z12DeInitGameUIv   # DeInitGameUI()
  __Z12InitTagPanelv   # InitTagPanel()
  __Z13InitCashPanelv   # InitCashPanel()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14DeInitTagPanelv   # DeInitTagPanel()
  __Z15DeInitCashPanelv   # DeInitCashPanel()
  __Z16CreateStaticTagsv   # CreateStaticTags()
  __Z16DeInitStaffPanelv   # DeInitStaffPanel()
  __Z16InitCoasterPanelv   # InitCoasterPanel()
  __Z16InitControlPanelv   # InitControlPanel()
  __Z16UI_ClearSelectedv   # UI_ClearSelected()
  __Z17DestroyStaticTagsv   # DestroyStaticTags()
  __Z18DeInitCoasterPanelv   # DeInitCoasterPanel()
  __Z18DeInitControlPanelv   # DeInitControlPanel()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20InstantRemoveAllTagsv   # InstantRemoveAllTags()
  __ZN10CMMDefault11OnLeftClickEii   # CMMDefault::OnLeftClick(int, int)
  __ZN10CMMDefault12OnRightClickEii   # CMMDefault::OnRightClick(int, int)
  __ZN10CMMDefault7GetModeEv   # CMMDefault::GetMode()
  __ZN10CMMDefault8OnAssignEv   # CMMDefault::OnAssign()
  __ZN10CMMDefaultC1Ev   # CMMDefault::CMMDefault()
  __ZN10CMMDefaultD1Ev   # CMMDefault::~CMMDefault()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11UIInterface10DeInitGameEv   # UIInterface::DeInitGame()
  __ZN11UIInterface8InitGameEv   # UIInterface::InitGame()
  __ZN15InterfaceWindow6unlinkEv   # InterfaceWindow::unlink()
  __ZN15InterfaceWindow9setCursorEs   # InterfaceWindow::setCursor(short)
  __ZN6CMouse15AssignMouseModeEP10CMouseMode   # CMouse::AssignMouseMode(CMouseMode*)
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
  _gCashPanel
  _gCashPanelScript
  _gCoasterPanelScript
  _gControlPanel
  _gControlPanelScript
  _gFirstPersonWindowScript
  _gGameWindowCentreX
  _gGameWindowCentreY
  _gGameWindowHalfHeight
  _gGameWindowHalfWidth
  _gMouse
  _gMouseMode
  _gMouseWord1E0
  _gMouseWord1E4
  _gStaffPanelA
  _gStaffPanelB
  _gStaffPanelC
  _gStaticTags
  _gTagPanel
  _gTagPanelScript
  _gTagPanelSource
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUISelectedObject
  _gUISelectedPeep
  _gUISelectedThing
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
  __Z19UI_UseInterfaceSizeb   # UI_UseInterfaceSize(bool)
)
set(TPW_UNIT_GameUIInit_OWN_CALLLOG ON)
