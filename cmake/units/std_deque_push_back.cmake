# Unit v5243: std::deque<errorthingy>::push_back and grow_buf_back (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std_deque_push_back.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_deque_push_back src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_deque_push_back_KEEP
  __Z23MslErrorDeque_push_backP13MslErrorDequePKj   # MslErrorDeque_push_back(MslErrorDeque*, unsigned int const*)
  __Z27MslErrorDeque_grow_buf_backP13MslErrorDequejPSt8__cdequeIPKvNSt3__19allocatorIS3_EEEPjj   # MslErrorDeque_grow_buf_back(MslErrorDeque*, unsigned int, std::__cdeque<void const*, std::__1::allocator<void const*>>*, unsigned int*, unsigned int)
  _gMslErrorDequePad
)
