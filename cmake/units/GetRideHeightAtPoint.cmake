# Unit v5228: ride height by polygon search (src/game/park/TPHField.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetRideHeightAtPoint.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetRideHeightAtPoint src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_GetRideHeightAtPoint_KEEP
  __Z16DoNodePolySearchP9tag_sNodeP23sEmbeddedDataNodeParamsffP8s3DPointi   # DoNodePolySearch(tag_sNode*, sEmbeddedDataNodeParams*, float, float, s3DPoint*, int)
  __Z20GetRideHeightAtPointffRf   # GetRideHeightAtPoint(float, float, float&)
  __Z23GetHeightFromPolySearchffii   # GetHeightFromPolySearch(float, float, int, int)
  _gRideCtxRef
  _gRideFlags
)
