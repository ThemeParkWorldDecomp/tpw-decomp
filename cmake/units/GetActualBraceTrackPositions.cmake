# Unit v5562: the brace placement of the track editor (GetActualBraceTrackPositions, GetActualBraceTangents, GetTangentLengths;
# src/GetActualBraceTrackPositions.cpp). The callees outside the unit are recording fakes in tests/unit_GetActualBraceTrackPositions.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_GetActualBraceTrackPositions src/GetActualBraceTrackPositions.cpp platform/arena.cpp)
