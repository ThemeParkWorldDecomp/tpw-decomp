# The map reset and the attribute map (unit w112c): LLCC_ResetMap, the map and track cell resets, HFieldCellStatus, RemoveNeighbour
# and ModifyDirection; the chunk file reader with its handlers, the list members it uses and CMap::LoadAttribMap; the script
# system's check table. The unit file includes calllog.cpp itself (it records the heap traffic of the list nodes and the table).
# platform/arena.cpp gives the height field's file image structures their Ptr32 targets.
set(TPW_UNIT_MapReset src/MapReset.cpp src/ChunkFile.cpp src/ScriptCheckTable.cpp platform/arena.cpp)
set(TPW_UNIT_MapReset_OWN_CALLLOG ON)
