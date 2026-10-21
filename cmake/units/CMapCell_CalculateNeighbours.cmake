# Unit v5632: CMapCell::CalculateNeighbours (src/game/park/MapCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CMapCell_CalculateNeighbours.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CMapCell_CalculateNeighbours src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_CalculateNeighbours_KEEP
  __ZN8CMapCell19CalculateNeighboursE10CELL_MODESiib   # CMapCell::CalculateNeighbours(CELL_MODES, int, int, bool)
  _gCalcNeighboursIdMinusX
  _gCalcNeighboursIdMinusY
  _gCalcNeighboursIdPlusX
  _gCalcNeighboursIdPlusY
  _gCalcNeighboursJoinEntry
  _gCalcNeighboursPrevA
  _gCalcNeighboursPrevB
)
