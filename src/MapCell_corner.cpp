// CMapCell::IsCorner 0x1009c7fc (main app, SimTheme Park 2.04b3; lobby.cpp on the Mac, the inventory's file for this range; unit w204): the
// twin of CTrackCell::IsCorner (src/TrackCell_flags.cpp).
#include <cstdint>

#include "MapCell.h"

// 0x1009c7fc, 156 bytes: the neighbour byte (GetNeighbours is read first) when exactly two of the four horizontal and vertical neighbours are set and
// they are at a right angle; the tests run in the listing's order: 1 and 4, 1 and 0x40, 0x10 and 4, 0x10 and 0x40. Else 0.
uint8_t CMapCell::IsCorner() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 1) != 0 && (neighbours & 0x40) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}
