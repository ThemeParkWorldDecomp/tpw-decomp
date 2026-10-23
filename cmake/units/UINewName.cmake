# Unit w43b: the name entry dialog of the login panel (NewName), the Dialog window maker, InterfaceWindow::setZ and GetClientRect, and the check
# buttons' SetCurrent. Everything they call is a fake that records in the unit file; the rectangles are real.
set(TPW_UNIT_UINewName src/game/ui/ui_playerLogin.cpp src/game/ui/ui_ErrorDisplay.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/InterfaceButton.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UINewName_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z6DialogPsPFiP15InterfaceWindowiiiEi   # Dialog(short*, int (*)(InterfaceWindow*, int, int, int), int)
  __Z7NewNamei   # NewName(int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN15InterfaceWindow13GetClientRectEP8CoolRect   # InterfaceWindow::GetClientRect(CoolRect*)
  __ZN15InterfaceWindow13SetClientRectEP8CoolRect   # InterfaceWindow::SetClientRect(CoolRect*)
  __ZN15InterfaceWindow4setZEs   # InterfaceWindow::setZ(short)
  __ZN21InterfaceCheckButtons10GetCurrentEv   # InterfaceCheckButtons::GetCurrent()
  __ZN21InterfaceCheckButtons10SetCurrentEi   # InterfaceCheckButtons::SetCurrent(int)
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
  _gNewNameText
  _gNewNameWindow
  _gNewNameWindowData
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
  __Z13UI_CentreDownPK15InterfaceWindow   # UI_CentreDown(InterfaceWindow const*)
)
