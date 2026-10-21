# Unit v5563: CPerson::ProcessStateWalksToMajor and CPerson::DoSomeThinking (src/CPerson_..._CPerson_ProcessStateWalksToMajor.cpp). The callees outside the unit are recording fakes in
# tests/unit_CPerson_ProcessStateWalksToMajor.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_ProcessStateWalksToMajor src/game/people/Person.cpp src/game/people/PersonQueue.cpp
    platform/arena.cpp)
set(TPW_UNIT_CPerson_ProcessStateWalksToMajor_KEEP
  __ZN7CPerson14DoSomeThinkingEv   # CPerson::DoSomeThinking()
  __ZN7CPerson24ProcessStateWalksToMajorEv   # CPerson::ProcessStateWalksToMajor()
  _gNoMajorCell
  _gNoMajorRide
)
