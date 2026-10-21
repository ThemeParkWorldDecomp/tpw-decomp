# Unit v5675: GenerateRideHeightMap (src/game/park/TPHField.cpp). The callees outside the unit are recording fakes in
# tests/unit_GenerateRideHeightMap.cpp, which also defines the globals of other files.
set(TPW_UNIT_GenerateRideHeightMap src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_GenerateRideHeightMap_KEEP
  __Z21GenerateRideHeightMapP16tag_sRideAnimSetjjP9RideShapei   # GenerateRideHeightMap(tag_sRideAnimSet*, unsigned int, unsigned int, RideShape*, int)
  _gRideHeightRows
)
