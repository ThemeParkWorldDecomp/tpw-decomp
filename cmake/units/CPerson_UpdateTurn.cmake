# Unit v5217: CPerson::UpdateTurn and CPerson::FindNearestFireworks (src/game/people/PersonQueue.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_UpdateTurn.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_UpdateTurn src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_UpdateTurn_KEEP
  __ZN7CPerson10UpdateTurnEv   # CPerson::UpdateTurn()
  __ZN7CPerson20FindNearestFireworksEv   # CPerson::FindNearestFireworks()
  _gQueueLinkA
  _gQueueLinkB
  _gThingListEnd
)
