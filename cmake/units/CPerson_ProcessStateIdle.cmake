# Unit v5098: CPerson::ProcessStateIdle (src/game/people/PersonQueue.cpp). The callees outside the unit
# are recording fakes in tests/unit_CPerson_ProcessStateIdle.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_ProcessStateIdle src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_ProcessStateIdle_KEEP
  __ZN7CPerson16ProcessStateIdleEv   # CPerson::ProcessStateIdle()
  __ZNK14CMsgPersonIdle7GetTypeEv   # CMsgPersonIdle::GetType() const
  _gLitterKindCount
  _gNoBalloonThing
  _gNoDecision
  _gNoFireworks
  _gNoLitterBin
)
