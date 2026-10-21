# Unit w104: the way out of the lobby. CGame's theme setters and ConverttoCurrentTheme, KeyParticlePOUFFE, DeInitLobby and DeInitLobbyUI,
# IsRideFirstPersonRide, ESP_RemoveSprites, the online globe's destructor, Close and list helpers. Real: those, the rectangle
# (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp) and the light (src/game/Light.cpp). The strings' copy constructor, destructor and assignment, the master save, the map,
# the windows, the emitter and sound calls, the sprite file and list, the screen base class and the globe's entries are recording
# fakes in the unit file, which includes tests/calllog.cpp itself (it keeps the blocks the program frees).
set(TPW_UNIT_LobbyExit src/game/Game.cpp src/game/TPCamera.cpp src/game/ui/UIInterface.cpp
    src/game/ui/ui_islandLobbyController.cpp src/game/ui/ui_main.cpp src/game/HLSprite.cpp
    src/game/online/lobby_globe.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp
    src/game/Light.cpp platform/arena.cpp)
set(TPW_UNIT_LobbyExit_KEEP
  __Z13DeInitLobbyUIv   # DeInitLobbyUI()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z17ESP_RemoveSpritesv   # ESP_RemoveSprites()
  __Z17KeyParticlePOUFFEv   # KeyParticlePOUFFE()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21ConverttoCurrentThemei   # ConverttoCurrentTheme(int)
  __Z21IsRideFirstPersonRidej   # IsRideFirstPersonRide(unsigned int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11UIInterface11DeInitLobbyEv   # UIInterface::DeInitLobby()
  __ZN29CLBScreenComponentOnlineGlobe21DeleteCityListEntriesEv   # CLBScreenComponentOnlineGlobe::DeleteCityListEntries()
  __ZN29CLBScreenComponentOnlineGlobe22DeleteGlobeListEntriesEv   # CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries()
  __ZN29CLBScreenComponentOnlineGlobe5CloseEv   # CLBScreenComponentOnlineGlobe::Close()
  __ZN29CLBScreenComponentOnlineGlobeD1Ev   # CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe()
  __ZN5CGame15SetCurrentThemeERK23TbDynamicStringTemplateIcE   # CGame::SetCurrentTheme(TbDynamicStringTemplate<char> const&)
  __ZN5CGame15SetCurrentThemeEi   # CGame::SetCurrentTheme(int)
  __ZN5CGame19SetCurrentThemeNameEv   # CGame::SetCurrentThemeName()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
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
)
set(TPW_UNIT_LobbyExit_OWN_CALLLOG ON)
