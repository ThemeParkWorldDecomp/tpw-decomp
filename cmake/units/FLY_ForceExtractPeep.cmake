# Unit v5980: the fly bases' peep handling and repair state (src/game/rides/FlyingRide.cpp). The callees outside the unit are
# recording fakes in tests/unit_FLY_ForceExtractPeep.cpp, which also defines the globals of other files.
set(TPW_UNIT_FLY_ForceExtractPeep src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_ForceExtractPeep_KEEP
  __Z11FLY_AddPeepii   # FLY_AddPeep(int, int)
  __Z15FLY_ExtractPeepi   # FLY_ExtractPeep(int)
  __Z18FLY_SetRepairStateii   # FLY_SetRepairState(int, int)
  __Z20FLY_ForceExtractPeepi   # FLY_ForceExtractPeep(int)
)
