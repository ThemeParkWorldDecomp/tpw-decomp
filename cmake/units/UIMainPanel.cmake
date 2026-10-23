# The UI main panel helpers (unit w101): SetPanel, CheckPanel, SetTitleText, DefaultPanelMsg, UISound, the two advisor hooks and
# GetFullyOutOfFirstPerson. CoolRect is the real one (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp); every other callee is a recording fake in the unit file.
set(TPW_UNIT_UIMainPanel src/game/ui/ui_main.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp
    src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIMainPanel_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UIMain_UISoundi   # UIMain_UISound(int)
  __Z15UIMain_SetPanelP15InterfaceWindow   # UIMain_SetPanel(InterfaceWindow*)
  __Z17UIMain_CheckPanelv   # UIMain_CheckPanel()
  __Z19UIMain_SetTitleTextP15InterfaceWindowiPDs   # UIMain_SetTitleText(InterfaceWindow*, int, char16_t*)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21UIMain_AdvisorMessage14AdvisorMessage   # UIMain_AdvisorMessage(AdvisorMessage)
  __Z22UIMain_DefaultPanelMsgP15InterfaceWindowiii   # UIMain_DefaultPanelMsg(InterfaceWindow*, int, int, int)
  __Z27UIMain_CancelAdvisorMessage14AdvisorMessage   # UIMain_CancelAdvisorMessage(AdvisorMessage)
  __Z31UIMain_GetFullyOutOfFirstPersonv   # UIMain_GetFullyOutOfFirstPerson()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
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
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
)
