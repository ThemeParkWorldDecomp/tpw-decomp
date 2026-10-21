# Unit v5553: the download globe's destructor and Hide (src/game/online/lobby_globe.cpp). The callees outside the unit are recording fakes in
# tests/unit_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.cpp, which also defines the globes of other files.
set(TPW_UNIT_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo src/game/online/lobby_globe.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo_KEEP
  __ZN31CLBScreenComponentDownloadGlobe4HideEv   # CLBScreenComponentDownloadGlobe::Hide()
  __ZN31CLBScreenComponentDownloadGlobeD1Ev   # CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe()
)
