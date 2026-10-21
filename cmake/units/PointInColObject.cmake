# Unit v5367: the collision object tests (PointInColObject, DoAreasIntersect, ClosestIntersectingColObjectToPoint;
# src/game/rides/BumperRide.cpp). PointOutAllInverse is a recording fake in tests/unit_PointInColObject.cpp,
# which also defines gColList.
set(TPW_UNIT_PointInColObject src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_PointInColObject_KEEP
  __Z16DoAreasIntersectP12tabCOLOBJECTS0_   # DoAreasIntersect(tabCOLOBJECT*, tabCOLOBJECT*)
  __Z16PointInColObjectiiP12tabCOLOBJECT   # PointInColObject(int, int, tabCOLOBJECT*)
  __Z35ClosestIntersectingColObjectToPointP12tabCOLOBJECTiii   # ClosestIntersectingColObjectToPoint(tabCOLOBJECT*, int, int, int)
)
