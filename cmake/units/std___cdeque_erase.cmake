# Unit v5149: the const void* deque's erase, resize and push_back (src/game/MslSupport.cpp). The callees outside the unit are recording
# fakes in tests/unit_std___cdeque_erase.cpp, which also defines the globals of other files.
set(TPW_UNIT_std___cdeque_erase src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___cdeque_erase_KEEP
  __ZNSt8__cdequeIPKvNSt3__19allocatorIS1_EEE5eraseE13MslCdequeIterS6_   # std::__cdeque<void const*, std::__1::allocator<void const*>>::erase(MslCdequeIter, MslCdequeIter)
  __ZNSt8__cdequeIPKvNSt3__19allocatorIS1_EEE6resizeEjS1_   # std::__cdeque<void const*, std::__1::allocator<void const*>>::resize(unsigned int, void const*)
  __ZNSt8__cdequeIPKvNSt3__19allocatorIS1_EEE9push_backERKS1_   # std::__cdeque<void const*, std::__1::allocator<void const*>>::push_back(void const* const&)
)
