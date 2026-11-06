set(TPW_UNIT_CTrackCell_Modify src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_Modify_KEEP
  __ZN10CTrackCell10ModifyBodyE10CELL_MODESii   # CTrackCell::ModifyBody(CELL_MODES, int, int)
  __ZN10CTrackCell12ModifyLockedEv   # CTrackCell::ModifyLocked()
  __ZN10CTrackCell6ModifyE10CELL_MODESii   # CTrackCell::Modify(CELL_MODES, int, int)
  _gLLCC4854
  _gTrackModifyNoParent
  _gTrackModifyNoThing
)
