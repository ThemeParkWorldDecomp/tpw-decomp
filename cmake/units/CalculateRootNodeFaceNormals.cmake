# Unit v5338: ride mesh helpers (CalculateRootNodeFaceNormals, GetBlendedBaseTextures, GetNodeWorldVertPosnsIntoGeomBuffer; src/game/rides/TPRide.cpp).
# The callees outside the unit are recording fakes in tests/unit_CalculateRootNodeFaceNormals.cpp, which also defines the globals of other files.
set(TPW_UNIT_CalculateRootNodeFaceNormals src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_CalculateRootNodeFaceNormals_KEEP
  __Z22GetBlendedBaseTexturesP16tag_sRideAnimSet   # GetBlendedBaseTextures(tag_sRideAnimSet*)
  __Z28CalculateRootNodeFaceNormalsP9tag_sNodeiRi   # CalculateRootNodeFaceNormals(tag_sNode*, int, int&)
  __Z35GetNodeWorldVertPosnsIntoGeomBufferP9tag_sNode   # GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode*)
)
