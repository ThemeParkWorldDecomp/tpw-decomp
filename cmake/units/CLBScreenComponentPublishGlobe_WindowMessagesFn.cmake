# Unit v5957: the publish globe's WindowMessagesFn, Show, Hide, CreateCities, HandleOnlineBacktrack and the download globe's
# CreateCities and HandleOnlineBacktrack (src/game/online/lobby_globe.cpp). The callees outside the unit
# are recording fakes in tests/unit_CLBScreenComponentPublishGlobe_WindowMessagesFn.cpp, which also defines the globals of other files.
set(TPW_UNIT_CLBScreenComponentPublishGlobe_WindowMessagesFn src/game/online/lobby_globe.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentPublishGlobe_WindowMessagesFn_KEEP
  __ZN30CLBScreenComponentPublishGlobe12CreateCitiesEP9TbPtrListI13CFSOnlineCityE   # CLBScreenComponentPublishGlobe::CreateCities(TbPtrList<CFSOnlineCity>*)
  __ZN30CLBScreenComponentPublishGlobe16WindowMessagesFnEiii   # CLBScreenComponentPublishGlobe::WindowMessagesFn(int, int, int)
  __ZN30CLBScreenComponentPublishGlobe21HandleOnlineBacktrackEv   # CLBScreenComponentPublishGlobe::HandleOnlineBacktrack()
  __ZN30CLBScreenComponentPublishGlobe4HideEv   # CLBScreenComponentPublishGlobe::Hide()
  __ZN30CLBScreenComponentPublishGlobe4ShowEv   # CLBScreenComponentPublishGlobe::Show()
  __ZN31CLBScreenComponentDownloadGlobe12CreateCitiesEP9TbPtrListI13CFSOnlineCityE   # CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>*)
  __ZN31CLBScreenComponentDownloadGlobe21HandleOnlineBacktrackEv   # CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack()
)
