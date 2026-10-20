// CTrackCell (main app, SimTheme Park 2.04b3; the inventory files its code under TrainingCentre.cpp, as TrackCell.h line 1 says: unreliable; unit w179):
// HasBeenModifiedFrom 0x1012fb74, Serialise 0x1012fbc0 and the two GetNeighbouringCell 0x1012fcb8 (by offset) and 0x1012fddc (by compass direction).
//
// Quirks kept: Serialise throws away CCellBase::Serialise's answer (as CMapCell's); the track neighbour's cell lies at the SCALED offset (the
// listing reads the product's copy, 0x50(r1) and 0x54(r1), for the validity test AND the cell), where CMapCell::GetNeighbouringCell uses the
// unscaled delta for the cell; the cell is found from gWorld + 0x2d8 + 0x110000 (not gWorldMap); a direction byte that is not one single compass
// bit asserts and answers NULL.
#include <cstddef>
#include <cstdint>

#include "Debug.h"
#include "ObjectParts.h"
#include "Serialise.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

namespace {

constexpr const char* kNameSegmentNumber = "mSegmentNumber";   // the Mac's string pool at 0x102469c8
constexpr const char* kBadDirection = "Incorrect use of function CTrackCell::GetNeighbouringCell( Direction , offset)";   // 0x102469d7 (no final newline)

}  // namespace

// 0x1012fb74, 8 bytes.
int32_t CTrackCell::HasBeenModifiedFrom(const CTrackCell& other) {
    (void)other;
    return 1;
}

// 0x1012fbc0, 196 bytes: the shared part first (its answer is ignored), then for 0 <= version < 2 the segment number, saved when `save` is nonzero.
int32_t CTrackCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    CCellBase::Serialise(file, save, version, unused);
    if (version >= 2 || version < 0) return 0;
    if (save != 0) {
        if (SAVEVAR(file, &mSegmentNumber, kNameSegmentNumber, 0) != 0) return 1;
    } else {
        if (LOADVAR(file, &mSegmentNumber, kNameSegmentNumber, 0) != 0) return 1;
    }
    return 0;
}

// 0x1012fcb8, 228 bytes. The product of the delta and the offset serves for the validity test and for the cell.
CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    const MapDelta scaled = delta * offset;
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    if (!CMap::IsXYValid(static_cast<int32_t>((i & 0x7fu) + static_cast<uint32_t>(scaled.mX)),
                         static_cast<int32_t>((i >> 7) + static_cast<uint32_t>(scaled.mY)))) return nullptr;
    const uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(scaled.mX) + (static_cast<uint32_t>(scaled.mY) << 7) + mCellId);
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 + 0x110000 + (static_cast<int32_t>(id) - 1) * 0x28);
}

// 0x1012fddc, 412 bytes: the compass delta of the direction bit, then the offset form.
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    const MapDelta* source;
    switch (direction) {
        case 1: source = &gMapDeltaMinusY; break;
        case 2: source = &gMapDeltaPlusXMinusY; break;
        case 4: source = &gMapDeltaPlusX; break;
        case 8: source = &gMapDeltaPlusXPlusY; break;
        case 0x10: source = &gMapDeltaPlusY; break;
        case 0x20: source = &gMapDeltaMinusXPlusY; break;
        case 0x40: source = &gMapDeltaMinusX; break;
        case 0x80: source = &gMapDeltaMinusXMinusY; break;
        default:
            GAME_ASSERTMSG(0, kBadDirection);
            return nullptr;
    }
    MapDelta local = *source;
    return GetNeighbouringCell(local, offset);
}
