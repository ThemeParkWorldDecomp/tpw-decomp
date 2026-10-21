# Unit v5087: the bump removal (BMP_REMOVEBUMP, BMP_DOCK, RemovePeepsFromBump, FindTargetBump; src/game/rides/BumperRide.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_REMOVEBUMP.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_REMOVEBUMP src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_REMOVEBUMP_KEEP
  __Z14BMP_REMOVEBUMPii   # BMP_REMOVEBUMP(int, int)
  __Z14FindTargetBumpP7tagBUMPP11tagBUMPRIDEPi   # FindTargetBump(tagBUMP*, tagBUMPRIDE*, int*)
  __Z19RemovePeepsFromBumpP7tagBUMPi   # RemovePeepsFromBump(tagBUMP*, int)
  __Z8BMP_DOCKP7tagBUMP   # BMP_DOCK(tagBUMP*)
  _gAdvisorThingId
)
