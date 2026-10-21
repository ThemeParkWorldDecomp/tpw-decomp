set(TPW_UNIT_CTrackCell_Modify src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_Modify_KEEP
  __ZN10CTrackCell10ModifyBodyE10CELL_MODESii   # CTrackCell::ModifyBody(CELL_MODES, int, int)
  __ZN10CTrackCell12ModifyLockedEv   # CTrackCell::ModifyLocked()
  __ZN10CTrackCell14ModifyTypeBodyE10CELL_MODES   # CTrackCell::ModifyTypeBody(CELL_MODES)
  __ZN10CTrackCell6ModifyE10CELL_MODESii   # CTrackCell::Modify(CELL_MODES, int, int)
  _gLLCC4854
  _gLLCCDC21C
  _gTrackModifyNoParent
  _gTrackModifyNoThing
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
