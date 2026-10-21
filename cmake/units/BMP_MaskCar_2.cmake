# Unit v5926: the bump AI, thrust, mask and buoy target (src/game/rides/BumperRide.cpp). The callees outside the unit are
# recording fakes in tests/unit_BMP_MaskCar_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_MaskCar_2 src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_MaskCar_2_KEEP
  __Z10BMP_ThrustP7tagBUMPi   # BMP_Thrust(tagBUMP*, int)
  __Z11BMP_MaskCarP7tagBUMPi   # BMP_MaskCar(tagBUMP*, int)
  __Z17SetNextBuoyTargetP7tabBUOYP7tagBUMP   # SetNextBuoyTarget(tabBUOY*, tagBUMP*)
  __Z6BMP_AIP7tagBUMP   # BMP_AI(tagBUMP*)
  _gBumpAiMaxStep
  _gBumpAiMinDist
)
