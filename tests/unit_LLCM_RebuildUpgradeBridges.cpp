// Test library for LLCM_CoverUpgrade and LLCM_RebuildUpgradeBridges (unit v5059; libtpw_unit_LLCM_RebuildUpgradeBridges.dylib), main app:
//   src/game/park/LLCellMouse.cpp
//
// The callees (cell lookups, flags, neighbours, the line drawer) are fakes that record their arguments (pointers as offsets from the world
// block) and answer from the test's queues. gGameWorld and the compass offsets are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld;
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};
const MapDelta gMapDeltaMinusY = {0, -1, 0};
const MapDelta gMapDeltaPlusXMinusY = {1, -1, 0};
const MapDelta gMapDeltaMinusXPlusY = {-1, 1, 0};
const MapDelta gMapDeltaMinusX = {-1, 0, 0};
const MapDelta gMapDeltaMinusXMinusY = {-1, -1, 0};

namespace {

uintptr_t Base() { return reinterpret_cast<uintptr_t>(gGameWorld); }
int64_t Off(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - Base())); }
void* At(int64_t off) { return off == 0 ? nullptr : reinterpret_cast<void*>(Base() + static_cast<uintptr_t>(off)); }

}  // namespace

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {Off(this)});
    return static_cast<CTrackCell*>(At(ret("te")));
}

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {Off(this)});
    return static_cast<CMapCell*>(At(ret("me")));
}

int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {Off(this)});
    return static_cast<int32_t>(ret("tx"));
}

int32_t CTrackCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {Off(this), flag});
    return static_cast<int32_t>(ret("flag"));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Off(this)});
    return static_cast<int32_t>(ret("trk"));
}

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCell", {Off(this), delta.mX, delta.mY, offset});
    return static_cast<CTrackCell*>(At(ret("nb")));
}

void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawTrackLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
}

extern "C" {

uint64_t tpw_llcm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llcm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llcm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapDelta));
    out[n++] = static_cast<int64_t>(offsetof(MapDelta, mY));
    return n;
}

void tpw_llcm_cover(uint64_t x, uint64_t y) {
    LLCM_CoverUpgrade(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(x)), reinterpret_cast<int32_t*>(static_cast<uintptr_t>(y)));
}

void tpw_llcm_rebuild(int32_t x, int32_t y) { LLCM_RebuildUpgradeBridges(x, y); }

}  // extern "C"
