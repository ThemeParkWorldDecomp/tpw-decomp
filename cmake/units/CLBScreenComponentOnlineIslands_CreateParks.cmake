# Unit v5639: the online islands screen's CreateParks, Show and ProcessCancel (src/game/online/lobby_island.cpp).
# The callees outside the unit are recording fakes in tests/unit_CLBScreenComponentOnlineIslands_CreateParks.cpp, which also defines the globals of other files.
set(TPW_UNIT_CLBScreenComponentOnlineIslands_CreateParks src/game/online/lobby_island.cpp platform/arena.cpp)
set(TPW_UNIT_CLBScreenComponentOnlineIslands_CreateParks_KEEP
  __ZN31CLBScreenComponentOnlineIslands11CreateParksEP9TbPtrListI13CFSOnlineParkE   # CLBScreenComponentOnlineIslands::CreateParks(TbPtrList<CFSOnlinePark>*)
  __ZN31CLBScreenComponentOnlineIslands13ProcessCancelEv   # CLBScreenComponentOnlineIslands::ProcessCancel()
  __ZN31CLBScreenComponentOnlineIslands4ShowEv   # CLBScreenComponentOnlineIslands::Show()
)
