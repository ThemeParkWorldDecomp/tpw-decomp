# Unit v5269: the coaster queues and script commands (src/game/rides/CoasterQueue.cpp). The callees outside the unit are recording fakes in
# tests/unit_AllocateQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_AllocateQueue src/game/rides/CoasterQueue.cpp platform/arena.cpp)
set(TPW_UNIT_AllocateQueue_KEEP
  __Z13AllocateQueueP16sWraparoundQueuej   # AllocateQueue(sWraparoundQueue*, unsigned int)
  __Z14CoasterAddPeepjj   # CoasterAddPeep(unsigned int, unsigned int)
  __Z14CoasterGetPeepj   # CoasterGetPeep(unsigned int)
  __Z15CoasterSetSpeedjj   # CoasterSetSpeed(unsigned int, unsigned int)
  __Z20CoasterGetQueueSpacej   # CoasterGetQueueSpace(unsigned int)
  __Z21CoasterSetRepairStatejj   # CoasterSetRepairState(unsigned int, unsigned int)
  __Z25CoasterSetOpenClosedStateji   # CoasterSetOpenClosedState(unsigned int, int)
  __Z25HACKCoasterReturnLostPeepP20tag_sCoasterInstancej   # HACKCoasterReturnLostPeep(tag_sCoasterInstance*, unsigned int)
  _gCoasterQueueSpaceLimit
)
