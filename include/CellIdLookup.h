// CellIdLookup.h: a map or track cell found by its 1 based id with plain arithmetic, as the Mac does it (MapCellOf and
// TrackCellOf in src/game/ui/ui_main.cpp, PointerCell in src/game/Mouse.cpp, the ticket queue in
// src/game/people/PersonQueue.cpp). The Mac computes (id - 1) * size in 32 bits and adds it to a 32 bit address, so the
// id 0 (no cell: a cell that belongs to nothing, the pointer off the map) is the cell before the first one, still inside
// CWorld: the map cells start at CWorld + 0x2d8, so map cell 0 is CWorld + 0x294, and the track cells follow the map
// cells, so track cell 0 is the last 0x28 bytes of the last map cell. On a 64 bit host the same 32 bit offset taken as
// unsigned is 4 GB past the cells instead.
// Map cell 0's thing list head (+0x24) is CWorld + 0x2b8, the high half of CParkTime's raw year (+0x18: 0 when loaded,
// then 2000 and on). On the big-endian Mac that half is 0, the end of a thing list, so CMapCell::GetObjectThingId and every
// walk of the list answer no thing. A little-endian host reads the low half there, the year itself, as a thing id, so the
// port answers no thing for map cell 0 without reading it. Header only and pure, so the host tests see what the game does.
#pragma once
