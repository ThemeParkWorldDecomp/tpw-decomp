# Unit v5954: the person's ticket booth walk (src/game/people/PersonQueue.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_LeaveQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_LeaveQueue src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_LeaveQueue_KEEP
  __ZN7CPerson10LeaveQueueEv   # CPerson::LeaveQueue()
  __ZN7CPerson20FeelingAboutEntryFeeEv   # CPerson::FeelingAboutEntryFee()
  __ZN7CPerson25ProcessStatePayingAtBoothEv   # CPerson::ProcessStatePayingAtBooth()
  __ZN7CPerson32ProcessStateWaitingForParkToOpenEv   # CPerson::ProcessStateWaitingForParkToOpen()
  __ZN7CPerson33ProcessStateWalkingToTicketBoothsEv   # CPerson::ProcessStateWalkingToTicketBooths()
  _gPersonTableBusSprite
)
