# Unit v5268: CTrackCell::IsTrackStartBlock, IsBridge, GetSegmentNumber, ValidTypeChange (src/game/park/TrackCell.cpp).
# GetMapCellEquiv and IsFlag are recording fakes in tests/unit_CTrackCell_IsTrackStartBlock.cpp, which also defines gGameWorld.
set(TPW_UNIT_CTrackCell_IsTrackStartBlock src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_IsTrackStartBlock_KEEP
  __ZN10CTrackCell15ValidTypeChangeE10CELL_MODES   # CTrackCell::ValidTypeChange(CELL_MODES)
  __ZNK10CTrackCell16GetSegmentNumberEv   # CTrackCell::GetSegmentNumber() const
  __ZNK10CTrackCell17IsTrackStartBlockEv   # CTrackCell::IsTrackStartBlock() const
  __ZNK10CTrackCell8IsBridgeEv   # CTrackCell::IsBridge() const
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
