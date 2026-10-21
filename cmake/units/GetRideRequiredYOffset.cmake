# Unit v5886: ride placement (GetRideRequiredYOffset and five more; src/game/rides/RidePlacement.cpp). The callees outside the
# unit are recording fakes in tests/unit_GetRideRequiredYOffset.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetRideRequiredYOffset src/game/rides/RidePlacement.cpp platform/arena.cpp)
set(TPW_UNIT_GetRideRequiredYOffset_KEEP
  __Z22GetRideMasterBoundAreajP11sAlignedBox   # GetRideMasterBoundArea(unsigned int, sAlignedBox*)
  __Z22GetRideRequiredYOffsetiiPviiji   # GetRideRequiredYOffset(int, int, void*, int, int, unsigned int, int)
  __Z26CorrectRidePosnForRotationPiS_iS_S_ii   # CorrectRidePosnForRotation(int*, int*, int, int*, int*, int, int)
  __Z26SetRidePositionAndRotationP16tag_sRideAnimSetiiiiifjii   # SetRidePositionAndRotation(tag_sRideAnimSet*, int, int, int, int, int, float, unsigned int, int, int)
  __Z35GetNodeWorldVertPosnsIntoGeomBufferP9tag_sNodeP7sMatrix   # GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode*, sMatrix*)
  __Z8MoveRideP16tag_sRideAnimSetiiPviij   # MoveRide(tag_sRideAnimSet*, int, int, void*, int, int, unsigned int)
)
