// Test library for LLCR_FixBridgesReverse (unit v51010; libtpw_unit_LLCR_FixBridgesReverse.dylib, tests/test_LLCR_FixBridgesReverse.py), main app:
//   src/LLCR_FixBridgesReverse.cpp   LLCR_FixBridgesReverse
//
// Every callee (the CTrackCell predicates and setters, OPPOSITE, LLCR_OffsetEndOfTrack, SetCurRideAnim, Start/StopFlashingObject) is a fake
// that records its arguments (cells as their index in the world's track grid, ride sets as labels) and answers from the test's queues.
// The globals of other files the function reads (gGameWorld, gRideAnimSets, gLLCCE0CC) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CWorld* gGameWorld;
tag_sRideAnimSet* gRideAnimSets[8];
uint8_t gLLCCE0CC;

namespace {

int gPhase = 0;   // 1 between IsBridge and the next IsCorner: GetNeighbouringCell then never answers NULL (the original dereferences it)

__attribute__((noinline)) int64_t CellIdx(const void* p) {
    if (p == nullptr) return -1;
    return (reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(&gGameWorld->mTrackCells[0])) / 0x28;
}

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}
int32_t CTrackCell::IsJustTrack() const {
    ev("IsJustTrack", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsJustTrack"));
}
uint8_t CTrackCell::IsCorner() const {
    gPhase = 0;
    ev("IsCorner", {CellIdx(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}
int32_t CTrackCell::IsBridge() const {
    gPhase = 1;
    ev("IsBridge", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsBridge"));
}
int32_t CTrackCell::IsTrackStartBlock() const {
    ev("IsTrackStartBlock", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsTrackStartBlock"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {CellIdx(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {CellIdx(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    int64_t here = CellIdx(this);
    int64_t v = ret("Nbr");
    if (gPhase == 1 && v == 0) v = 1;
    CTrackCell* result = nullptr;
    int64_t idx = -1;
    if (v != 0) {
        idx = here + v;
        if (idx < 0 || idx >= 0x4000) idx = here;
        result = &gGameWorld->mTrackCells[idx];
    }
    ev("GetNeighbouringCell", {here, direction, offset, idx});
    return result;
}
void CTrackCell::SetFlag(uint16_t flags) { ev("SetFlag", {CellIdx(this), flags}); }
void CTrackCell::UnSetFlag(uint16_t flags) { ev("UnSetFlag", {CellIdx(this), flags}); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {CellIdx(this)}); }

uint8_t OPPOSITE(uint8_t direction) {
    uint8_t result = static_cast<uint8_t>((direction << 4) | (direction >> 4));
    ev("OPPOSITE", {direction, result});
    return result;
}

uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetEnd", {*x, *y, rotation});
    *x += static_cast<int32_t>(ret("OffX"));
    *y += static_cast<int32_t>(ret("OffY"));
    return static_cast<uint8_t>(ret("OffDir"));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, CanonBits(speed), b});
}
void StartFlashingObject(tag_sRideAnimSet* set) { ev("StartFlashing", {lab(set)}); }
void StopFlashingObject(tag_sRideAnimSet* set) { ev("StopFlashing", {lab(set)}); }

extern "C" {

uint64_t tpw_fbr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"sets", &gRideAnimSets[0]},
        {"e0cc", &gLLCCE0CC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fbr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The grid base (the world, allocated once) and the grid's first cell.
uint64_t tpw_fbr_world() {
    static CWorld* world = nullptr;
    if (!world) world = static_cast<CWorld*>(tpw::ArenaAllocZeroed(sizeof(CWorld)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(world));
}

int32_t tpw_fbr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    return n;
}

uint32_t tpw_fbr_run(uint64_t ride, int32_t a, int32_t b, int32_t c) {
    return static_cast<uint32_t>(LLCR_FixBridgesReverse(reinterpret_cast<CObject*>(static_cast<uintptr_t>(ride)), a != 0, b != 0, c != 0));
}

}  // extern "C"
