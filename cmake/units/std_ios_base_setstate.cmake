# Unit v5523: std::ios_base::setstate (src/game/MslSupport.cpp). __throw is a recording fake in
# tests/unit_std_ios_base_setstate.cpp, which also defines the failure vtable globals.
set(TPW_UNIT_std_ios_base_setstate src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_ios_base_setstate_KEEP
  __Z14MslIosSetstateP10MacIosBasei   # MslIosSetstate(MacIosBase*, int)
)
