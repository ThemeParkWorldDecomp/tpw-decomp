# Unit v5061: CalculateMaxVertexHeights (src/game/park/TPHField.cpp). The callees outside the unit are recording fakes in
# tests/unit_CalculateMaxVertexHeights.cpp. List only the sources the library must compile.
set(TPW_UNIT_CalculateMaxVertexHeights src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_CalculateMaxVertexHeights_KEEP
  __Z25CalculateMaxVertexHeightsP9tag_sNodeP15tag_sM3D2Header7sMatrixP14sRideHeightMapPS0_P15tag_sAnimHeaderiPi   # CalculateMaxVertexHeights(tag_sNode*, tag_sM3D2Header*, sMatrix, sRideHeightMap*, tag_sNode**, tag_sAnimHeader*, int, int*)
  _gRideHeightLimitX
  _gRideHeightLimitZ
  _gRideHeightScan
  _gRideHeightStride
)
