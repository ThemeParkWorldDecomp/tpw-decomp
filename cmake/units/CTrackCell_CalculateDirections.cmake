# Unit v5696: CTrackCell::CalculateDirections and GetTile (src/game/park/TrackCell.cpp). The callees outside the unit are
# recording fakes in tests/unit_CTrackCell_CalculateDirections.cpp, which also defines the globals of other files.
set(TPW_UNIT_CTrackCell_CalculateDirections src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_CalculateDirections_KEEP
  __ZN10CTrackCell19CalculateDirectionsEii   # CTrackCell::CalculateDirections(int, int)
  __ZNK10CTrackCell7GetTileEPi   # CTrackCell::GetTile(int*) const
  _gTrackTileCountA
  _gTrackTileCountB
  _gTrackTileLast
  _gTrackTileRand
  _gTrackTileTableA
  _gTrackTileTableB
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
