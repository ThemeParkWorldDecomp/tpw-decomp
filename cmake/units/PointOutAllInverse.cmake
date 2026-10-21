# Unit v5112: the point tests over the collision list (src/game/rides/BumperRide.cpp). PointInColObject is a recording fake
# in tests/unit_PointOutAllInverse.cpp, which also defines gColList.
set(TPW_UNIT_PointOutAllInverse src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_PointOutAllInverse_KEEP
  __Z14NavPointInAreaP12tabCOLOBJECTPN10NS_MacDoze8tagPOINTE   # NavPointInArea(tabCOLOBJECT*, NS_MacDoze::tagPOINT*)
  __Z18PointOutAllInverseii   # PointOutAllInverse(int, int)
  __Z19PointInAnyColObjectii   # PointInAnyColObject(int, int)
)
