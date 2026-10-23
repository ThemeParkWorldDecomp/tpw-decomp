# The main panel (unit w118): CreateMainPanel, InitStaffPanel, the vertical gradient (constructor, destructor, CalcVerts, CalcUVs, Show), the number and
# cash text objects (constructors, destructors), SetOriginalPosition and the three small message functions, with the window scripts. The rectangles
# and the text sprite's constructor are real; the windows, the draw object base classes, the sprite parts, the text formatter, the mesh calls and the
# fonts are fakes in the unit file. Unit w131 adds platform/msl.cpp (swprintf, for GoldenStuffMsg) and the stand-ins its five handlers reach.
set(TPW_UNIT_MainPanel src/game/ui/ui_mainpanel2.cpp src/game/ui/ui_staffPanel.cpp src/game/ui/InterfaceDrawEngine.cpp
    src/game/ui/InterfaceTextObjectSprite.cpp src/game/ui/InterfaceWindow.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_MainPanel_KEEP
  __Z11HappyBarMsgP15InterfaceWindowiii   # HappyBarMsg(InterfaceWindow*, int, int, int)
  __Z12MainPanelMsgP15InterfaceWindowiii   # MainPanelMsg(InterfaceWindow*, int, int, int)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14BobbleCallbackP15InterfaceWindowiii   # BobbleCallback(InterfaceWindow*, int, int, int)
  __Z14GoldenStuffMsgP15InterfaceWindowiii   # GoldenStuffMsg(InterfaceWindow*, int, int, int)
  __Z14InitStaffPanelv   # InitStaffPanel()
  __Z15CreateMainPanelv   # CreateMainPanel()
  __Z18CashWindowCallbackP15InterfaceWindowiii   # CashWindowCallback(InterfaceWindow*, int, int, int)
  __Z18DateWindowCallbackP15InterfaceWindowiii   # DateWindowCallback(InterfaceWindow*, int, int, int)
  __Z18HappyGaugeCallbackP15InterfaceWindowiii   # HappyGaugeCallback(InterfaceWindow*, int, int, int)
  __Z19MainPanelButtonsMsgP15InterfaceWindowiii   # MainPanelButtonsMsg(InterfaceWindow*, int, int, int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN15InterfaceWindow19SetOriginalPositionEP8CoolRect   # InterfaceWindow::SetOriginalPosition(CoolRect*)
  __ZN25InterfaceNumberTextObjectC1EPv   # InterfaceNumberTextObject::InterfaceNumberTextObject(void*)
  __ZN25InterfaceNumberTextObjectD1Ev   # InterfaceNumberTextObject::~InterfaceNumberTextObject()
  __ZN25InterfaceTextObjectSpriteC1EPv   # InterfaceTextObjectSprite::InterfaceTextObjectSprite(void*)
  __ZN28InterfaceDrawEngineVGradient4ShowEs   # InterfaceDrawEngineVGradient::Show(short)
  __ZN28InterfaceDrawEngineVGradient7CalcUVsEv   # InterfaceDrawEngineVGradient::CalcUVs()
  __ZN28InterfaceDrawEngineVGradient9CalcVertsEv   # InterfaceDrawEngineVGradient::CalcVerts()
  __ZN28InterfaceDrawEngineVGradientC1EPv   # InterfaceDrawEngineVGradient::InterfaceDrawEngineVGradient(void*)
  __ZN28InterfaceDrawEngineVGradientD1Ev   # InterfaceDrawEngineVGradient::~InterfaceDrawEngineVGradient()
  __ZN31InterfaceCashAndGTickTextObjectC1EPv   # InterfaceCashAndGTickTextObject::InterfaceCashAndGTickTextObject(void*)
  __ZN31InterfaceCashAndGTickTextObjectD1Ev   # InterfaceCashAndGTickTextObject::~InterfaceCashAndGTickTextObject()
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
  _gAerialWindow
  _gCashWindowFrames
  _gCashWindowGuard
  _gCashWindowWord
  _gGoldenKeysGuard
  _gGoldenLastKeys
  _gGoldenLastSpending
  _gGoldenSpendGuard
  _gGoldenStuffPanel
  _gLastPanelFinance
  _gLastPanelLists
  _gLastPanelPark
  _gMainPanel
  _gMainPanelFontIndex
  _gMainPanelScript
  _gSlideoutTab
  _gStaffPanelScriptA
  _gStaffPanelScriptB
  _gStaffPanelState
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
set(TPW_UNIT_MainPanel_OWN_CALLLOG ON)
