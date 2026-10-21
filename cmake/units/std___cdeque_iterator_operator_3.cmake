# Unit v5925: cdeque iterator operator-=, queue<errorthingy>::~queue, __cdeque::reserve (src/game/MslSupport.cpp).
# The callees outside the unit are recording fakes in tests/unit_std___cdeque_iterator_operator_3.cpp, which also defines the globals.
set(TPW_UNIT_std___cdeque_iterator_operator_3 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___cdeque_iterator_operator_3_KEEP
  __Z16MslCdequeIterSubP13MslCdequeIteri   # MslCdequeIterSub(MslCdequeIter*, int)
  __Z20MslQueueErrorDestroyP13MslErrorDeques   # MslQueueErrorDestroy(MslErrorDeque*, short)
  __ZNSt8__cdequeIPKvNSt3__19allocatorIS1_EEE7reserveEj   # std::__cdeque<void const*, std::__1::allocator<void const*>>::reserve(unsigned int)
)
