# Unit v5571: the online globe's Tick and WindowMessagesFn (src/game/online/lobby_globe.cpp). The callees outside the
# unit are recording fakes in tests/unit_CLBScreenComponentOnlineGlobe_Tick.cpp, which also defines the globes of other files.
set(TPW_UNIT_CLBScreenComponentOnlineGlobe_Tick src/game/online/lobby_globe.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentOnlineGlobe_Tick_KEEP
  __ZN29CLBScreenComponentOnlineGlobe16WindowMessagesFnEiii   # CLBScreenComponentOnlineGlobe::WindowMessagesFn(int, int, int)
  __ZN29CLBScreenComponentOnlineGlobe4TickEv   # CLBScreenComponentOnlineGlobe::Tick()
  _gGlobeCentre
  _gGlobeViewTarget
)
