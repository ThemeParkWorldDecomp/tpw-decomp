# Unit v5562: the brace placement of the track editor (GetActualBraceTrackPositions, GetActualBraceTangents, GetTangentLengths;
# src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in tests/unit_GetActualBraceTrackPositions.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_GetActualBraceTrackPositions src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_GetActualBraceTrackPositions_KEEP
  __Z17GetTangentLengthsjP15tag_sTrackBracePfS1_iP8s3DPointS3_   # GetTangentLengths(unsigned int, tag_sTrackBrace*, float*, float*, int, s3DPoint*, s3DPoint*)
  __Z22GetActualBraceTangentsP15tag_sTrackBraceP16tag_sRideAnimSet   # GetActualBraceTangents(tag_sTrackBrace*, tag_sRideAnimSet*)
  __Z28GetActualBraceTrackPositionsP16tag_sRideAnimSetP15tag_sTrackBrace   # GetActualBraceTrackPositions(tag_sRideAnimSet*, tag_sTrackBrace*)
  _gBraceTrackScale2
)
