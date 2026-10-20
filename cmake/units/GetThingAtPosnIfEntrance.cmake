# Unit v51013: GetThingAtPosnIfEntrance, MoveCameraOverRideExit, FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide
# (src/lobby_exit_IsRideRideable_GetThingAtPosnIfEntrance.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetThingAtPosnIfEntrance.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetThingAtPosnIfEntrance src/lobby_exit_IsRideRideable_GetThingAtPosnIfEntrance.cpp platform/arena.cpp)
