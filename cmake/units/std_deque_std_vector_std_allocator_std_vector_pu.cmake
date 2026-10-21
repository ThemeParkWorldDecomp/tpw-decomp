# Unit v5938: the Metrowerks std::deque<std::vector<Argument>> (push_back, pop_front, copy constructor, make_iterator;
# src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_deque_std_vector_std_allocator_std_vector_pu.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_deque_std_vector_std_allocator_std_vector_pu src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_deque_std_vector_std_allocator_std_vector_pu_KEEP
  __Z25ArgumentDequeMakeIteratorP20MslArgumentDequeIterPK16MslArgumentDequej   # ArgumentDequeMakeIterator(MslArgumentDequeIter*, MslArgumentDeque const*, unsigned int)
  __ZN13online_callee21ArgumentDequePopFrontEPv   # online_callee::ArgumentDequePopFront(void*)
  __ZN13online_callee21ArgumentDequePushBackEPvPKv   # online_callee::ArgumentDequePushBack(void*, void const*)
  __ZN13online_callee9DequeCopyEPvPKv   # online_callee::DequeCopy(void*, void const*)
  _gArgDequeBlockSize
  _gArgDequeFill
)
