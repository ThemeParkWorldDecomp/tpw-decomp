# Unit v5278: GetActualBraceParams and UpdatePylonStackStructures (src/game/rides/CoasterPylon.cpp). GetAlignmentParamsSlow is a
# recording fake in tests/unit_GetActualBraceParams.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetActualBraceParams src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_GetActualBraceParams_KEEP
  __Z20GetActualBraceParamsv   # GetActualBraceParams()
  __Z26UpdatePylonStackStructuresP13sPylonControlP15sTrackBraceData   # UpdatePylonStackStructures(sPylonControl*, sTrackBraceData*)
)
