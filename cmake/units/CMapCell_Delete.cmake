# Unit v5211: CMapCell::Delete (src/game/park/MapCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CMapCell_Delete.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMapCell_Delete src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_Delete_KEEP
  __ZN8CMapCell6DeleteEii   # CMapCell::Delete(int, int)
  _gDeleteNoParent
  _gDeleteNoThing
)
