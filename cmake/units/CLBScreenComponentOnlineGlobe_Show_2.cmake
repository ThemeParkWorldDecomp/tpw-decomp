# Unit v5923: the online globe's Show, Hide, zoom, CreateCities, UnRegister (src/game/online/lobby_globe.cpp).
# The callees outside the unit are recording fakes in tests/unit_CLBScreenComponentOnlineGlobe_Show_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CLBScreenComponentOnlineGlobe_Show_2 src/game/online/lobby_globe.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentOnlineGlobe_Show_2_KEEP
  __ZN29CLBScreenComponentOnlineGlobe10UnRegisterEP16CLBGlobeCityFlag   # CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobeCityFlag*)
  __ZN29CLBScreenComponentOnlineGlobe11GlobeZoomInEv   # CLBScreenComponentOnlineGlobe::GlobeZoomIn()
  __ZN29CLBScreenComponentOnlineGlobe12CreateCitiesEP9TbPtrListI13CFSOnlineCityE   # CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>*)
  __ZN29CLBScreenComponentOnlineGlobe12GlobeZoomOutEv   # CLBScreenComponentOnlineGlobe::GlobeZoomOut()
  __ZN29CLBScreenComponentOnlineGlobe20SortOutZoomConstantsEv   # CLBScreenComponentOnlineGlobe::SortOutZoomConstants()
  __ZN29CLBScreenComponentOnlineGlobe4HideEv   # CLBScreenComponentOnlineGlobe::Hide()
  __ZN29CLBScreenComponentOnlineGlobe4ShowEv   # CLBScreenComponentOnlineGlobe::Show()
  __ZN8CLBGlobeD1Ev   # CLBGlobe::~CLBGlobe()
  TPW_GLOBE_SHOWHIDE_REAL@game/online/lobby_globe.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/online/lobby_globe.cpp (a guard macro; src/game/online/lobby_globe.cpp had it)
)
