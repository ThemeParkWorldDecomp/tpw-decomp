# Unit v5011: GetTangentsFromZVector (src/game/rides/CoasterPylon.cpp). The MSL imports are recording fakes in
# tests/unit_GetTangentsFromZVector.cpp, which also defines gTPGlobals.
set(TPW_UNIT_GetTangentsFromZVector src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_GetTangentsFromZVector_KEEP
  __Z22GetTangentsFromZVectorP16tag_sCoasterTypeP8s3DPointP18tag_sBezierControlffP13sPylonControlS2_S2_ii   # GetTangentsFromZVector(tag_sCoasterType*, s3DPoint*, tag_sBezierControl*, float, float, sPylonControl*, s3DPoint*, s3DPoint*, int, int)
)
