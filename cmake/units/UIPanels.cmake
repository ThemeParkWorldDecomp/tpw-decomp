# Unit w131: the park's UI panel message functions (MainPanelButtonsMsg, CashPanelMsg, StaffPanelMsg, HappyBarMsg, ControlPanelMsg, StaffLocMsg,
# CashWindowCallback, BobbleCallback, ModifiedCheckButtonsCallback, GoldenStuffMsg), UIInterface::CheckMouseContext and CIntrface::Process. The
# rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp) and the 16-bit C library (swprintf) are real; the windows, the master save, the world, the analyser and every panel the handlers open are fakes in the
# unit file.
set(TPW_UNIT_UIPanels src/game/ui/ui_mainpanel2.cpp src/game/ui/ui_cashPanel.cpp src/game/ui/ui_controlPanel.cpp
    src/game/ui/ui_staffPanel.cpp src/game/ui/UIInterface.cpp src/game/ui/intrface.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp platform/msl.cpp)
set(TPW_UNIT_UIPanels_KEEP
  __Z11HappyBarMsgP15InterfaceWindowiii   # HappyBarMsg(InterfaceWindow*, int, int, int)
  __Z11StaffLocMsgP15InterfaceWindowiii   # StaffLocMsg(InterfaceWindow*, int, int, int)
  __Z12CashPanelMsgP15InterfaceWindowiii   # CashPanelMsg(InterfaceWindow*, int, int, int)
  __Z12MainPanelMsgP15InterfaceWindowiii   # MainPanelMsg(InterfaceWindow*, int, int, int)
  __Z13StaffPanelMsgP15InterfaceWindowiii   # StaffPanelMsg(InterfaceWindow*, int, int, int)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14BobbleCallbackP15InterfaceWindowiii   # BobbleCallback(InterfaceWindow*, int, int, int)
  __Z14GoldenStuffMsgP15InterfaceWindowiii   # GoldenStuffMsg(InterfaceWindow*, int, int, int)
  __Z15ControlPanelMsgP15InterfaceWindowiii   # ControlPanelMsg(InterfaceWindow*, int, int, int)
  __Z18CashWindowCallbackP15InterfaceWindowiii   # CashWindowCallback(InterfaceWindow*, int, int, int)
  __Z18DateWindowCallbackP15InterfaceWindowiii   # DateWindowCallback(InterfaceWindow*, int, int, int)
  __Z18HappyGaugeCallbackP15InterfaceWindowiii   # HappyGaugeCallback(InterfaceWindow*, int, int, int)
  __Z19MainPanelButtonsMsgP15InterfaceWindowiii   # MainPanelButtonsMsg(InterfaceWindow*, int, int, int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z28ModifiedCheckButtonsCallbackP15InterfaceWindowiii   # ModifiedCheckButtonsCallback(InterfaceWindow*, int, int, int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11UIInterface17CheckMouseContextEi   # UIInterface::CheckMouseContext(int)
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
  __ZN9CIntrface7ProcessEv   # CIntrface::Process()
  _gCashWindowFrames
  _gCashWindowGuard
  _gCashWindowWord
  _gGoldenKeysGuard
  _gGoldenLastKeys
  _gGoldenLastSpending
  _gGoldenSpendGuard
  _gStaffPanelThing
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
)
set(TPW_UNIT_UIPanels_OWN_CALLLOG ON)
