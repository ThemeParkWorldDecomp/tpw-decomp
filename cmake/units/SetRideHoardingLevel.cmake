# Unit v5987: the ride hoardings (src/game/park/TPHoardings.cpp). The callees outside the unit are recording fakes in
# tests/unit_SetRideHoardingLevel.cpp, which also defines gTPGlobals.
set(TPW_UNIT_SetRideHoardingLevel src/game/park/TPHoardings.cpp platform/arena.cpp)
set(TPW_UNIT_SetRideHoardingLevel_KEEP
  __Z17ActivateHoardingsP16tag_sRideAnimSetj   # ActivateHoardings(tag_sRideAnimSet*, unsigned int)
  __Z19EnableRideHoardingsP16tag_sRideAnimSeti   # EnableRideHoardings(tag_sRideAnimSet*, int)
  __Z19ReactivateHoardingsP16tag_sRideAnimSet   # ReactivateHoardings(tag_sRideAnimSet*)
  __Z20SetRideHoardingLevelP16tag_sRideAnimSetf   # SetRideHoardingLevel(tag_sRideAnimSet*, float)
)
