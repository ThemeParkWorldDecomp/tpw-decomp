set(TPW_UNIT_CMapCell_Modify src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_Modify_KEEP
  __ZN8CMapCell6ModifyE10CELL_MODESii   # CMapCell::Modify(CELL_MODES, int, int)
  _gModifyLastMode
  _gModifyNoParent
  _gModifyNoThing
  _gModifyPylonIndex
  _gModifyTypeRefused
)
