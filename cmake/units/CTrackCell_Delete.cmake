# Unit v5990: CTrackCell::Delete (src/game/park/TrackCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CTrackCell_Delete.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CTrackCell_Delete src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_Delete_KEEP
  __ZN10CTrackCell6DeleteEiib   # CTrackCell::Delete(int, int, bool)
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
