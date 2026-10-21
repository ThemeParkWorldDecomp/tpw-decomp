# Unit v5695: COnlineControl::ProcessQueue, AddBuddy and LoadBuddy (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_ProcessQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_ProcessQueue src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_ProcessQueue_KEEP
  __ZN14COnlineControl12ProcessQueueEv   # COnlineControl::ProcessQueue()
  __ZN14COnlineControl8AddBuddyERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEEi   # COnlineControl::AddBuddy(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&, int)
  __ZN14COnlineControl9LoadBuddyEPKc   # COnlineControl::LoadBuddy(char const*)
)
