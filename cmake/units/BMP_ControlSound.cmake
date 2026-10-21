# Unit v5966: BMP_ControlSound, FreeBuoysForRide, BMP_GetCarsOnRide, BMP_SetRideModelSelection (src/game/rides/BumperRide.cpp). The callees
# outside the unit are recording fakes in tests/unit_BMP_ControlSound.cpp, which also defines the globals of other files.
set(TPW_UNIT_BMP_ControlSound src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_ControlSound_KEEP
  __Z16BMP_ControlSoundP7tagBUMP   # BMP_ControlSound(tagBUMP*)
  __Z16FreeBuoysForRideP11tagBUMPRIDEi   # FreeBuoysForRide(tagBUMPRIDE*, int)
  __Z17BMP_GetCarsOnRidei   # BMP_GetCarsOnRide(int)
  __Z25BMP_SetRideModelSelectioniii   # BMP_SetRideModelSelection(int, int, int)
)
