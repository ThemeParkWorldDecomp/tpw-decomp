# The map's core (unit w139): the six CWorld::Register overloads, CMap::Init, CMapCell::SetHFieldTile, MapDelta::Rotate (and the three compass offsets),
# GetMapCellFromPointerWorldPosn and CTrackCell::ModifyType. Every callee outside them is a fake in the unit file.
set(TPW_UNIT_MapCells src/World_register.cpp src/Map_init.cpp src/MapCell_tile.cpp src/MapDelta.cpp src/TPEngine_pointedcell.cpp src/TrackCell_modify.cpp platform/arena.cpp)
