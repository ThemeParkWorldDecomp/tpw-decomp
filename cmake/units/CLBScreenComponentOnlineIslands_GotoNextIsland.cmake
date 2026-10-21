# Unit v5255: the online islands screen's small members (src/game/online/lobby_island.cpp). The callees outside the unit are
# recording fakes in tests/unit_CLBScreenComponentOnlineIslands_GotoNextIsland.cpp, which also defines the globals of other files.
set(TPW_UNIT_CLBScreenComponentOnlineIslands_GotoNextIsland src/game/online/lobby_island.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentOnlineIslands_GotoNextIsland_KEEP
  __ZN31CLBScreenComponentOnlineIslands14GotoNextIslandEv   # CLBScreenComponentOnlineIslands::GotoNextIsland()
  __ZN31CLBScreenComponentOnlineIslands19SelectCurrentIslandEv   # CLBScreenComponentOnlineIslands::SelectCurrentIsland()
  __ZN31CLBScreenComponentOnlineIslands20DownloadFailFunctionEv   # CLBScreenComponentOnlineIslands::DownloadFailFunction()
  __ZN31CLBScreenComponentOnlineIslands21HandleOnlineBacktrackEv   # CLBScreenComponentOnlineIslands::HandleOnlineBacktrack()
  __ZN31CLBScreenComponentOnlineIslands24EnterCurrentIslandOnlineEv   # CLBScreenComponentOnlineIslands::EnterCurrentIslandOnline()
  __ZN31CLBScreenComponentOnlineIslands4TickEv   # CLBScreenComponentOnlineIslands::Tick()
)
