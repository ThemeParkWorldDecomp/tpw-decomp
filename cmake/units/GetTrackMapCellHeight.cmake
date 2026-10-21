# Unit v5070: GetTrackMapCellHeight (src/game/park/TrackCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetTrackMapCellHeight.cpp, which also defines the compass offsets.
set(TPW_UNIT_GetTrackMapCellHeight src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_GetTrackMapCellHeight_KEEP
  __Z21GetTrackMapCellHeightP8CMapCellfii   # GetTrackMapCellHeight(CMapCell*, float, int, int)
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
