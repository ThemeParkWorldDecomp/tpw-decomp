# Unit v5529: three states of the person (ProcessStateWalksToLitterBin, ProcessStateStandingInQueue, ProcessStateWaitingForScriptToAdmitMe;
# src/CPerson_..._CPerson_ProcessStateWalksToLitterBin.cpp). The callees outside the unit are recording fakes in
# tests/unit_CPerson_ProcessStateWalksToLitterBin.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_ProcessStateWalksToLitterBin src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_ProcessStateWalksToLitterBin_KEEP
  __ZN7CPerson27ProcessStateStandingInQueueEv   # CPerson::ProcessStateStandingInQueue()
  __ZN7CPerson28ProcessStateWalksToLitterBinEv   # CPerson::ProcessStateWalksToLitterBin()
  __ZN7CPerson37ProcessStateWaitingForScriptToAdmitMeEv   # CPerson::ProcessStateWaitingForScriptToAdmitMe()
  _gNoAdmitState
  _gNoLitterRide
)
