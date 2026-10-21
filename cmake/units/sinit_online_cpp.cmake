# Unit v5907: online.cpp's static initialiser, CheckShifts and the cdeque assign (src/game/online/online.cpp, src/game/ui/ui_worldLobbyController.cpp).
# The callees outside the unit are recording fakes in tests/unit_sinit_online_cpp.cpp, which also defines the globals of other files.
set(TPW_UNIT_sinit_online_cpp src/game/online/online.cpp src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_online_cpp_KEEP
  __Z11CheckShiftsv   # CheckShifts()
  __Z18__sinit_online_cppv   # __sinit_online_cpp()
  __Z26CdequeDoAssignArgumentIterPSt8__cdequeIPKvNSt3__19allocatorIS1_EEE13MslCdequeIterS7_   # CdequeDoAssignArgumentIter(std::__cdeque<void const*, std::__1::allocator<void const*>>*, MslCdequeIter, MslCdequeIter)
  _gOnlineSinitMask
  _gOnlineSinitNarrow
  _gOnlineSinitNodes
  _gOnlineSinitWide
  _gOnlineSinitZero
)
