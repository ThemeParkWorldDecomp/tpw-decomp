# Unit v5192: CTrackCell::CalculateNeighbours (src/game/park/TrackCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CTrackCell_CalculateNeighbours.cpp, which also defines the globals of other files.
set(TPW_UNIT_CTrackCell_CalculateNeighbours src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_CalculateNeighbours_KEEP
  __ZN10CTrackCell19CalculateNeighboursE10CELL_MODESii   # CTrackCell::CalculateNeighbours(CELL_MODES, int, int)
  _gTrackNbr290
  _gTrackNbr292
  _gTrackNbr294
  _gTrackNbr296
  _gTrackNbr298
  _gTrackNbr29A
  _gTrackNbr29C
  _gTrackNbr29E
  _gTrackNbrOn485C
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
