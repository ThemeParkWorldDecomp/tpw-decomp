# Unit v5115: the CodeWarrior std::deque<errorthingy> (pop_front, ~deque, make_iterator; src/game/MslSupport.cpp). The callees
# are recording fakes in tests/unit_std_deque_pop_front.cpp.
set(TPW_UNIT_std_deque_pop_front src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_deque_pop_front_KEEP
  __Z20MslErrorDequeDestroyP13MslErrorDeques   # MslErrorDequeDestroy(MslErrorDeque*, short)
  __ZN13MslErrorDeque9pop_frontEv   # MslErrorDeque::pop_front()
  __ZNK13MslErrorDeque13make_iteratorEP21MslErrorDequeIteratorj   # MslErrorDeque::make_iterator(MslErrorDequeIterator*, unsigned int) const
)
