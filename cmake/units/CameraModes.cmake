# Unit w141: the rest of TPCamera.cpp (src/game/TPCamera.cpp: camera modes, first person walk and ride, camera save and load) and
# MAP_wall_in_way (src/game/park/Map.cpp). Every callee outside the unit is a recording fake in tests/unit_CameraModes.cpp, which also
# defines the globals of other files.
set(TPW_UNIT_CameraModes src/game/TPCamera.cpp src/game/park/Map.cpp platform/arena.cpp)
set(TPW_UNIT_CameraModes_KEEP
  __Z10LoadCameraPv   # LoadCamera(void*)
  __Z10SaveCameraPv   # SaveCamera(void*)
  __Z13SetCameraModeiiiiPii   # SetCameraMode(int, int, int, int, int*, int)
  __Z15MAP_wall_in_wayhhh7NavMode   # MAP_wall_in_way(unsigned char, unsigned char, unsigned char, NavMode)
  __Z24Setup1stPersonRideMeshesi   # Setup1stPersonRideMeshes(int)
  __Z25ToggleRideRideIn1stPersoniiiPiii   # ToggleRideRideIn1stPerson(int, int, int, int*, int, int)
  __Z26SetupRideRideMatrixAndSuchii   # SetupRideRideMatrixAndSuch(int, int)
  __Z27ToggleFirstPersonWalkAroundv   # ToggleFirstPersonWalkAround()
  _gCameraHeadsRideSet
  _gCameraSavedPosition
  _gCameraSavedYaw
  _gEntranceWalkEnd
  _gFirstPersonRideFlagWord
  _gFirstPersonRideNode
  _gRideRideOnceGuard
  _gRideRideOnceValue
  __ZL19kCameraPositionInit   # kCameraPositionInit (static object)
)
