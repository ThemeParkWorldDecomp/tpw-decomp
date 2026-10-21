# Unit v5150: a person in a ride's queue (src/game/people/PersonQueue.cpp). The callees outside the unit are recording fakes in
# tests/unit_CPerson_DismissFromQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_DismissFromQueue src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_DismissFromQueue_KEEP
  __ZN7CPerson16DismissFromQueueEv   # CPerson::DismissFromQueue()
  __ZN7CPerson17EvaluateQueueEditEv   # CPerson::EvaluateQueueEdit()
  __ZN7CPerson20FindQueueDestinationEv   # CPerson::FindQueueDestination()
  __ZN7CPerson8ExitRideEv   # CPerson::ExitRide()
)
