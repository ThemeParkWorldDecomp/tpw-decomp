# Unit v5739: CServiceQueue (constructor, destructor, Close, Poll; src/sound/ServiceQueue.cpp). The glue
# imports and FillServiceBuffer are recording fakes in tests/unit_CServiceQueue_CServiceQueue.cpp.
set(TPW_UNIT_CServiceQueue_CServiceQueue src/sound/ServiceQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CServiceQueue_CServiceQueue_KEEP
  __ZN13CServiceQueue4PollEv   # CServiceQueue::Poll()
  __ZN13CServiceQueue5CloseEv   # CServiceQueue::Close()
  __ZN13CServiceQueueC1EP18TbSoundPipeChannelj   # CServiceQueue::CServiceQueue(TbSoundPipeChannel*, unsigned int)
  __ZN13CServiceQueueD1Ev   # CServiceQueue::~CServiceQueue()
  __ZN13CServiceQueuedlEPv   # CServiceQueue::operator delete(void*)
)
