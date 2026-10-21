# Unit v5546: CLBGlobeCityFlag constructor and destructor (src/game/online/lobby_globe.cpp). The callees outside the unit are
# recording fakes in tests/unit_CLBGlobeCityFlag_CLBGlobeCityFlag.cpp, which also defines gLobbyMeshes.
set(TPW_UNIT_CLBGlobeCityFlag_CLBGlobeCityFlag src/game/online/lobby_globe.cpp platform/arena.cpp)
set(TPW_UNIT_CLBGlobeCityFlag_CLBGlobeCityFlag_KEEP
  __ZN16CLBGlobeCityFlagC1EPK29CLBScreenComponentOnlineGlobePK13CFSOnlineCity   # CLBGlobeCityFlag::CLBGlobeCityFlag(CLBScreenComponentOnlineGlobe const*, CFSOnlineCity const*)
  __ZN16CLBGlobeCityFlagD1Ev   # CLBGlobeCityFlag::~CLBGlobeCityFlag()
  _gGlobeNoCityName
)
