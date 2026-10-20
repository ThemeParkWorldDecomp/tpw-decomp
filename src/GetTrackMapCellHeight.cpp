// GetTrackMapCellHeight 0x1009d8d0 (1756 bytes): the height a track piece takes from the map cell and its three neighbours.
#include <cstdint>

#include "MapCell.h"
#include "ObjectParts.h"

namespace {

// The cell types whose ground height counts: 4, 7, 9, 10, 0xb, 0x10 and 0x19 (the sets are disjoint). The height of the
// square is truncated, plus 9, divided by 10 (truncating) and scaled by 10.0f: a round up to the next multiple of 10.
bool UsesGround(int32_t type) {
    return type == 0xb || type == 0x10 || type == 0x19 || type == 7 || type == 4 || static_cast<uint32_t>(type - 9) < 2u;
}

float RoundedHeight(int32_t x, int32_t y) {
    const float height = GetWorldGSquareHeight(x, y, nullptr);
    const int32_t n = static_cast<int32_t>(height) + 9;
    const int32_t q = n / 10;
    return 10.0f * static_cast<float>(q);
}

float Neighbour(CMapCell* cell, const MapDelta& delta, float current) {
    MapDelta d;
    d.mX = delta.mX;
    d.mY = delta.mY;
    d.mFlag = 0;
    CMapCell* other = cell->GetNeighbouringCell(&d, 1);
    if (other == nullptr) return current;
    const uint32_t id = other->mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    if (UsesGround(static_cast<int32_t>(other->mType))) return RoundedHeight(x, y);
    return current;
}

}  // namespace

float GetTrackMapCellHeight(CMapCell* cell, float height, int32_t x, int32_t y) {
    if (UsesGround(static_cast<int32_t>(cell->mType))) height = RoundedHeight(x, y);
    height = Neighbour(cell, gMapDeltaPlusY, height);
    height = Neighbour(cell, gMapDeltaPlusX, height);
    height = Neighbour(cell, gMapDeltaPlusXPlusY, height);
    return height;
}
