# Unit v5062: the advisor's bump launch and collision (SetupHeadIDs, BMP_AddJoiningListToBump, BMP_LAUNCH, BMP_Collision; src/game/rides/BumperRide.cpp).
# The callees outside the unit are recording fakes in tests/unit_SetupHeadIDs.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetupHeadIDs src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_SetupHeadIDs_KEEP
  __Z10BMP_LAUNCHii   # BMP_LAUNCH(int, int)
  __Z12SetupHeadIDsP7tagBUMPi   # SetupHeadIDs(tagBUMP*, int)
  __Z13BMP_CollisionP7tagBUMP   # BMP_Collision(tagBUMP*)
  __Z24BMP_AddJoiningListToBumpi   # BMP_AddJoiningListToBump(int)
)
