# Proof unit (candidate 14): CTrackCell::ConnectionType with the two names linked as the shipped game links them, see tests/unit_CTrackCell_ConnectionType_Shipped.cpp.
set(TPW_UNIT_CTrackCell_ConnectionType_Shipped src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_ConnectionType_Shipped_KEEP
  __ZN10CTrackCell14ConnectionTypeE10CELL_MODESih   # CTrackCell::ConnectionType(CELL_MODES, int, unsigned char)
  _gLLCCDB40
)
