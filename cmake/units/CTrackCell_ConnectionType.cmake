# Unit v5629: CTrackCell::ConnectionType (src/game/park/TrackCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CTrackCell_ConnectionType.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CTrackCell_ConnectionType src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_ConnectionType_KEEP
  __ZN10CTrackCell14ConnectionTypeE10CELL_MODESih   # CTrackCell::ConnectionType(CELL_MODES, int, unsigned char)
  _gLLCCDB40
)
