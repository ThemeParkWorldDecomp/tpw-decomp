# Unit v51021: the ride pointer pick (RayPolyCheckOnRide, FindPointedRide; src/game/TPPointer.cpp). The callees outside
# the unit are recording fakes in tests/unit_RayPolyCheckOnRide.cpp, which also defines the globals of other files.
set(TPW_UNIT_RayPolyCheckOnRide src/game/TPPointer.cpp platform/arena.cpp)
set(TPW_UNIT_RayPolyCheckOnRide_KEEP
  __Z15FindPointedRidejjiffffP8s3DPointS0_f   # FindPointedRide(unsigned int, unsigned int, int, float, float, float, float, s3DPoint*, s3DPoint*, float)
  __Z18RayPolyCheckOnRideP9tag_sNode7sMatrix   # RayPolyCheckOnRide(tag_sNode*, sMatrix)
  _gPointedRideNormal
  _gRayDirection
  _gRayHitNormal
  _gRayHitPoint
  _gRayOrigin
)
