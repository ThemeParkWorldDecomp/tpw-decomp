# Unit v5976: std::deque<std::vector<Argument>>::grow_buf_back, destroy and the destructor (src/game/MslSupport.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_deque_std_vector_std_allocator_std_vector_gr.cpp.
set(TPW_UNIT_std_deque_std_vector_std_allocator_std_vector_gr src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_deque_std_vector_std_allocator_std_vector_gr_KEEP
  __Z20ArgumentDequeDestroyP16MslArgumentDeque20MslArgumentDequeIterS1_   # ArgumentDequeDestroy(MslArgumentDeque*, MslArgumentDequeIter, MslArgumentDequeIter)
  __Z21ArgumentDequeDestructP16MslArgumentDeques   # ArgumentDequeDestruct(MslArgumentDeque*, short)
  __ZN15argdeque_callee11GrowBufBackEP16MslArgumentDequejPvPjj   # argdeque_callee::GrowBufBack(MslArgumentDeque*, unsigned int, void*, unsigned int*, unsigned int)
  _gArgDequeGrowFill
)
