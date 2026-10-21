# The map's core (unit w139): the six CWorld::Register overloads, CMap::Init, CMapCell::SetHFieldTile, MapDelta::Rotate (and the three compass offsets),
# GetMapCellFromPointerWorldPosn and CTrackCell::ModifyType. Every callee outside them is a fake in the unit file.
set(TPW_UNIT_MapCells src/game/World.cpp src/game/park/Map.cpp src/game/park/MapCell.cpp src/game/park/MapDelta.cpp
    src/game/TPPointer.cpp src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_MapCells_KEEP
  __Z30GetMapCellFromPointerWorldPosnP8s3DPoint   # GetMapCellFromPointerWorldPosn(s3DPoint*)
  __ZN10CTrackCell10ModifyTypeE10CELL_MODES   # CTrackCell::ModifyType(CELL_MODES)
  __ZN4CMap4InitEv   # CMap::Init()
  __ZN6CWorld8RegisterEP11CResearcher   # CWorld::Register(CResearcher*)
  __ZN6CWorld8RegisterEP12CEntertainer   # CWorld::Register(CEntertainer*)
  __ZN6CWorld8RegisterEP6CGuard   # CWorld::Register(CGuard*)
  __ZN6CWorld8RegisterEP7CObject   # CWorld::Register(CObject*)
  __ZN6CWorld8RegisterEP9CHandyman   # CWorld::Register(CHandyman*)
  __ZN6CWorld8RegisterEP9CMechanic   # CWorld::Register(CMechanic*)
  __ZN8CMapCell13SetHFieldTileEv   # CMapCell::SetHFieldTile()
  __ZNK8MapDelta6RotateEi   # MapDelta::Rotate(int) const
  _gMapDeltaMinusX
  _gMapDeltaMinusXMinusY
  _gMapDeltaMinusXPlusY
  _gMapDeltaMinusY
  _gMapDeltaPlusX
  _gMapDeltaPlusXMinusY
  _gMapDeltaPlusXPlusY
  _gMapDeltaPlusY
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
