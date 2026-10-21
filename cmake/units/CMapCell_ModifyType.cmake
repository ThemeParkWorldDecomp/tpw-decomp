# Unit v5535: CMapCell::ModifyType and CMapCell::GetTile (src/game/park/MapCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CMapCell_ModifyType.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMapCell_ModifyType src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_ModifyType_KEEP
  __Z23CMapCell_ModifyTypeImplP8CMapCell10CELL_MODES   # CMapCell_ModifyTypeImpl(CMapCell*, CELL_MODES)
  __ZN8CMapCell10ModifyTypeE10CELL_MODES   # CMapCell::ModifyType(CELL_MODES)
  __ZN8CMapCell14ModifyTypeBodyE10CELL_MODES   # CMapCell::ModifyTypeBody(CELL_MODES)
  __ZN8CMapCell7GetTileEPi   # CMapCell::GetTile(int*)
  _gNoObjectThingId
  _gTileCountA
  _gTileCountB
  _gTileRandomFlag
  _gTileTableA
  _gTileTableB
)
