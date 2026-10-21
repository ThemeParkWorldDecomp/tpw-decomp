# Unit v51027: CMapCell::ConnectionType (src/game/park/MapCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CMapCell_ConnectionType.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMapCell_ConnectionType src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_ConnectionType_KEEP
  __ZN8CMapCell14ConnectionTypeE10CELL_MODESi   # CMapCell::ConnectionType(CELL_MODES, int)
)
