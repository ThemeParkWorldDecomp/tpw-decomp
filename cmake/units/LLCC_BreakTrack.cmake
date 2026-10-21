# Unit v5687: LLCC_BreakTrack, LLCC_GetTrackMeshAt (src/game/park/TrackCell.cpp). The callees outside the unit
# are recording fakes in tests/unit_LLCC_BreakTrack.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_BreakTrack src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_BreakTrack_KEEP
  __Z15LLCC_BreakTrackiih   # LLCC_BreakTrack(int, int, unsigned char)
  __Z19LLCC_GetTrackMeshAtii   # LLCC_GetTrackMeshAt(int, int)
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
