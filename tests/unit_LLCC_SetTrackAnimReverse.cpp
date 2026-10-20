// Test library for LLCC_SetTrackAnimReverse (unit v5684; libtpw_unit_LLCC_SetTrackAnimReverse.dylib, tests/test_LLCC_SetTrackAnimReverse.py), main app:
//   src/LLCellRide_offsets_LLCC_SetTrackAnimReverse.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. A cell pointer is logged
// as its index in the world's track cell array. gGameWorld and gRideAnimSets are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "CellBase.h"
#include "TrackCell.h"
#include "World.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
tag_sRideAnimSet* gRideAnimSets[32];

namespace {

int64_t CellIndex(const void* p) {
    return (reinterpret_cast<intptr_t>(p) - reinterpret_cast<intptr_t>(gGameWorld) - 0x1102d8) / 0x28;
}

int64_t SetIndex(const tag_sRideAnimSet* set) {
    for (int i = 0; i < 32; i++)
        if (gRideAnimSets[i] == set) return i;
    return -1;
}

}  // namespace

uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetEnd", {static_cast<int64_t>(*x), static_cast<int64_t>(*y), static_cast<int64_t>(rotation)});
    *x += static_cast<int32_t>(ret("OEdx"));
    *y += static_cast<int32_t>(ret("OEdy"));
    return static_cast<uint8_t>(ret("OffsetEnd"));
}

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {CellIndex(this)});
    return nullptr;
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {CellIndex(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {static_cast<int64_t>(direction)});
    return static_cast<uint8_t>(ret("OPPOSITE"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {CellIndex(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {SetIndex(set), static_cast<int64_t>(a), static_cast<int64_t>(anim), static_cast<int64_t>(loop), static_cast<int64_t>(fbits(speed)),
                          static_cast<int64_t>(b)});
}

CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("GetNeighbouringCell", {CellIndex(this), static_cast<int64_t>(direction), static_cast<int64_t>(offset)});
    int64_t v = ret("GetNeighbouringCell");
    if (v == 0) return nullptr;
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + (v - 1) * 0x28);
}

int32_t CTrackCell::IsTrackStartBlock() const {
    ev("IsTrackStartBlock", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsTrackStartBlock"));
}

extern "C" {

uint64_t tpw_llcc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"sets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llcc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llcc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

uint32_t tpw_llcc_run(uint64_t object, int32_t flag) {
    LLCC_SetTrackAnimReverse(reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)), flag != 0);
    return 0;
}

}  // extern "C"
