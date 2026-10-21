// Test library for GetTrackMapCellHeight (unit v5070; libtpw_unit_GetTrackMapCellHeight.dylib, tests/test_GetTrackMapCellHeight.py), main app:
//   src/game/park/TrackCell.cpp   GetTrackMapCellHeight
//
// The callees outside the unit are recording fakes: GetWorldGSquareHeight (answers from the queue "Height") and CMapCell::GetNeighbouringCell
// (answers the neighbour cell the case set for the offset asked: (0,1) first, (1,0) second, (1,1) third). The three compass offsets are
// defined here, as the program's definitions are in a file this library does not compile.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MapCell.h"
#include "ObjectParts.h"

using namespace calllog;

const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};

namespace {
CMapCell* gMain = nullptr;
CMapCell* gNeighbours[3] = {nullptr, nullptr, nullptr};
}  // namespace

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("Height", {x, y, static_cast<int64_t>(reinterpret_cast<uintptr_t>(extra))});
    const uint32_t bits = static_cast<uint32_t>(ret("Height"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    ev("Neighbour", {this == gMain ? 1 : 0, delta->mX, delta->mY, flag});
    if (delta->mX == 0 && delta->mY == 1) return gNeighbours[0];
    if (delta->mX == 1 && delta->mY == 0) return gNeighbours[1];
    if (delta->mX == 1 && delta->mY == 1) return gNeighbours[2];
    return nullptr;
}

extern "C" {

uint64_t tpw_v5070_addr(const char* name) {
    (void)name;
    return 0;
}

uint64_t tpw_v5070_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5070_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

void tpw_v5070_world(uint64_t main, uint64_t a, uint64_t b, uint64_t c) {
    gMain = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(main));
    gNeighbours[0] = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(a));
    gNeighbours[1] = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(b));
    gNeighbours[2] = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(c));
}

float tpw_v5070_run(uint64_t cell, float height, int32_t x, int32_t y) {
    return GetTrackMapCellHeight(reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell)), height, x, y);
}

}  // extern "C"
