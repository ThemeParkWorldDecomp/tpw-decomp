# Unit v51013: GetThingAtPosnIfEntrance, MoveCameraOverRideExit, FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide
# (src/game/TPCamera.cpp, src/game/rides/BumperRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetThingAtPosnIfEntrance.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetThingAtPosnIfEntrance src/game/TPCamera.cpp src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_GetThingAtPosnIfEntrance_KEEP
  __Z16FreeTrackForRideP11tagBUMPRIDE   # FreeTrackForRide(tagBUMPRIDE*)
  __Z18BMP_StartColObjectiii   # BMP_StartColObject(int, int, int)
  __Z22MoveCameraOverRideExitv   # MoveCameraOverRideExit()
  __Z24GetThingAtPosnIfEntranceii   # GetThingAtPosnIfEntrance(int, int)
  __Z25BMP_StartColObjectForRideP11tagBUMPRIDEiii   # BMP_StartColObjectForRide(tagBUMPRIDE*, int, int, int)
)
