// CTrackCell::IsBridge 0x100a34c4, IsTrackStartBlock 0x100a36cc, GetSegmentNumber 0x100a399c, ValidTypeChange 0x100a149c (unit v5268).
#include <cstdint>

#include "CellBase.h"
#include "TrackCell.h"
#include "World.h"

namespace {

// The track cell table of the world: gGameWorld + 0x1102d8, 0x28 bytes each, indexed by parent id - 1 (the id is a halfword, 0 gives -1).
const CTrackCell* StartBlockCell(const CTrackCell* cell) {
    const uint16_t id = cell->mParentID;
    const uintptr_t base = reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8;
    const uintptr_t address = base + static_cast<uintptr_t>(static_cast<intptr_t>((static_cast<int32_t>(id) - 1) * 0x28 + 0x110000));
    return reinterpret_cast<const CTrackCell*>(address);
}

inline bool IsTrackXType(int32_t type) { return type == 0xc || type == 0x11; }

}  // namespace

// 0x100a34c4: any of the four low flag bits.
int32_t CTrackCell::IsBridge() const {
    const uint16_t flags = mFlags;
    if ((flags & 1) != 0 || (flags & 2) != 0 || (flags & 4) != 0 || (flags & 8) != 0) return 1;
    return 0;
}

// 0x100a36cc: for a track X cell, the cell its parent id names counts when it is type 0xd or 0x12; then the cell itself.
int32_t CTrackCell::IsTrackStartBlock() const {
    if (IsTrackXType(static_cast<int32_t>(mType))) {
        const CTrackCell* parent = StartBlockCell(this);
        if (parent != nullptr) {
            const int32_t type = static_cast<int32_t>(parent->mType);
            if (type == 0xd || type == 0x12) return 1;
        }
    }
    const int32_t type = static_cast<int32_t>(mType);
    if (type == 0xd || type == 0x12) return 1;
    return 0;
}

// 0x100a399c: the segment number of the parent cell for a track X cell, else the cell's own.
int16_t CTrackCell::GetSegmentNumber() const {
    if (IsTrackXType(static_cast<int32_t>(mType))) return StartBlockCell(this)->mSegmentNumber;
    return mSegmentNumber;
}

// 0x100a149c: the map cell lookup's answer is unused.
int32_t CTrackCell::ValidTypeChange(CELL_MODES mode) {
    GetMapCellEquiv();
    const int32_t type = static_cast<int32_t>(mType);
    if (type == 0x19) return 1;
    if (type == 7) return 1;
    if (IsFlag(0x40) != 0) return 1;
    if (static_cast<int32_t>(mode) == 0) return 1;
    if (type == 0) return 1;
    if (static_cast<int32_t>(mode) == type) return 1;
    return 0;
}
