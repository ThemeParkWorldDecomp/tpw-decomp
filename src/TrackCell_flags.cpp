// CTrackCell's flag word and kind tests (main app, SimTheme Park 2.04b3; the inventory files this code under lobby.cpp / TrainingCentre.cpp: unreliable;
// unit w201): GetFlag 0x100a2998, SetFlag 0x100a2964, UnSetFlag 0x100a29cc, IsHoarding 0x100a35dc, IsTrackX 0x100a367c and IsCorner 0x100a37a0.
// SetFlag REPLACES the flag word (it does not OR the bits in): LLCC_LockHoarding reads the word, ORs 0x20 and stores it back.
#include <cstdint>

#include "TrackCell.h"

// 0x100a2998, 8 bytes.
uint16_t CTrackCell::GetFlag() { return mFlags; }

// 0x100a2964, 8 bytes.
void CTrackCell::SetFlag(uint16_t flags) { mFlags = flags; }

// 0x100a29cc, 16 bytes.
void CTrackCell::UnSetFlag(uint16_t flags) { mFlags = static_cast<uint16_t>(mFlags & ~flags); }

// 0x100a35dc, 28 bytes: the cell's mode is 0x19.
int32_t CTrackCell::IsHoarding() const { return mType == 0x19 ? 1 : 0; }

// 0x100a367c, 36 bytes: the cell's mode is 0xc, else 0x11.
int32_t CTrackCell::IsTrackX() const {
    if (mType == 0xc) return 1;
    return mType == 0x11 ? 1 : 0;
}

// 0x100a37a0, 156 bytes: the neighbour byte (GetNeighbours is read first) when exactly two of the four horizontal and vertical neighbours are set and
// they are at a right angle; the tests run in the listing's order: 1 and 4, 1 and 0x40, 0x10 and 4, 0x10 and 0x40. Else 0.
uint8_t CTrackCell::IsCorner() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 1) != 0 && (neighbours & 0x40) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}
