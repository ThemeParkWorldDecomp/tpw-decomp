// CTrackCell's shape tests the track walks ask (main app, SimTheme Park 2.04b3; lobby.cpp on the Mac, the inventory's file for this range;
// unit w204): IsStraight 0x100a3868 and IsCrossRoad 0x100a3914. Both read the neighbour byte first (GetNeighbours), then count the horizontal and
// vertical neighbours (NumberOfHVNeighbours); the neighbour byte is the answer when the shape fits, else 0. The compass bits: 1 north (y - 1),
// 4 east (x + 1), 0x10 south (y + 1), 0x40 west (x - 1).
#include <cstdint>

#include "TrackCell.h"

// 0x100a3868, 124 bytes: exactly two horizontal / vertical neighbours that lie in one line (1 with 0x10, or 4 with 0x40).
uint8_t CTrackCell::IsStraight() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 0x10) != 0) return neighbours;
    if ((neighbours & 4) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// 0x100a3914, 88 bytes: more than two horizontal / vertical neighbours (a signed compare).
uint8_t CTrackCell::IsCrossRoad() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() > 2) return neighbours;
    return 0;
}
