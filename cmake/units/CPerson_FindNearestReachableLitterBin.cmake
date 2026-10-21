# Unit v5245: CPerson queue states and nearest-thing searches (src/game/people/PersonQueue.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_FindNearestReachableLitterBin.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_FindNearestReachableLitterBin src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_FindNearestReachableLitterBin_KEEP
  __ZN7CPerson22FindNearestEntertainerEv   # CPerson::FindNearestEntertainer()
  __ZN7CPerson23ProcessStateBeingLoadedEv   # CPerson::ProcessStateBeingLoaded()
  __ZN7CPerson25ProcessStateBeingUnloadedEv   # CPerson::ProcessStateBeingUnloaded()
  __ZN7CPerson29FindNearestReachableLitterBinEj   # CPerson::FindNearestReachableLitterBin(unsigned int)
  _gEntertainerEnd
  _gLitterListEnd
  _gNoMinorDest
  _gNoQueueBack
)
