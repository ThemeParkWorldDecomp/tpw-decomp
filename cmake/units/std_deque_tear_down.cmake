# Unit v5680: std::deque<errorthingy> tear_down, copy constructor and destroy (src/game/MslSupport.cpp). The callees
# outside the unit are recording fakes in tests/unit_std_deque_tear_down.cpp, which also defines gMslMacAddressHigh.
set(TPW_UNIT_std_deque_tear_down src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_deque_tear_down_KEEP
  __Z21MslErrorDeque_destroyP13MslErrorDequePK21MslErrorDequeIteratorS3_   # MslErrorDeque_destroy(MslErrorDeque*, MslErrorDequeIterator const*, MslErrorDequeIterator const*)
  __ZN15errdeque_callee8TearDownEP13MslErrorDeque   # errdeque_callee::TearDown(MslErrorDeque*)
  __ZN15errsinit_callee9DequeCopyEPvPKv   # errsinit_callee::DequeCopy(void*, void const*)
  _gMslErrorDequeBlockSize
  _gMslErrorDequeFill
)
