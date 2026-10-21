// Test library for LLCR_FixBridges (unit v5887; libtpw_unit_LLCR_FixBridges_2.dylib, tests/test_LLCR_FixBridges_2.py), main app:
//   src/game/park/LLCellRide.cpp   LLCR_FixBridges
//
// The cell predicates, the neighbour lookup, the flag setters, OPPOSITE, LLCR_OffsetStartOfTrack and the ride animation calls are
// fakes that record their arguments. The cell predicates answer from scratch bytes of the cell (+0x14 track, +0x15 not just track,
// +0x16 corner, +0x17 bridge, +0x18 start block); the neighbour byte is the cell's +0xc. After 150 IsJustTrack calls the fakes turn
// the walk into one that stops at once, so no case loops forever.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
tag_sRideAnimSet* gRideAnimSets[8];
uint8_t gLLCCE0CC;

namespace {

int32_t gJustCalls = 0;
bool Panic() { return gJustCalls > 150; }

uint8_t* Base() { return reinterpret_cast<uint8_t*>(gGameWorld) + 0x1102d8; }

int64_t Idx(const void* p) {
    if (p == nullptr) return 0x7fffffff;
    return static_cast<int32_t>((reinterpret_cast<const uint8_t*>(p) - Base()) / 0x28);
}

uint8_t Byte(const CCellBase* c, int off) { return c == nullptr ? 0 : reinterpret_cast<const uint8_t*>(c)[off]; }

}  // namespace

uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetStart", {*x, *y, rotation});
    return static_cast<uint8_t>(ret("off"));
}

uint8_t OPPOSITE(uint8_t d) {
    ev("OPPOSITE", {d});
    return static_cast<uint8_t>((d << 4) | (d >> 4));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Idx(this)});
    return Panic() ? 0 : Byte(this, 0xc);
}

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Idx(this)});
    return __builtin_popcount(Byte(this, 0xc));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Idx(this)});
    return Byte(this, 0x14) != 0;
}

int32_t CTrackCell::IsJustTrack() const {
    ev("IsJustTrack", {Idx(this)});
    gJustCalls++;
    return Panic() ? 1 : Byte(this, 0x15) != 1;
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {Idx(this)});
    return Panic() ? 0 : Byte(this, 0x16);
}

int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {Idx(this)});
    return Byte(this, 0x17) != 0;
}

int32_t CTrackCell::IsTrackStartBlock() const {
    ev("IsTrackStartBlock", {Idx(this)});
    return Byte(this, 0x18) != 0;
}

void CTrackCell::SetFlag(uint16_t f) { ev("SetFlag", {Idx(this), f}); }
void CTrackCell::UnSetFlag(uint16_t f) { ev("UnSetFlag", {Idx(this), f}); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {Idx(this)}); }

CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t dir, int32_t off) {
    ev("GetNeighbouringCell", {Idx(this), dir, off});
    if (this == nullptr) return nullptr;
    int64_t i = Idx(this);
    if (i < 0) return nullptr;
    int32_t x = static_cast<int32_t>(i & 127), y = static_cast<int32_t>(i >> 7);
    switch (dir) {
        case 1: y -= off; break;
        case 4: x += off; break;
        case 0x10: y += off; break;
        case 0x40: x -= off; break;
        default: return nullptr;
    }
    if (x < 0 || x > 127 || y < 0 || y > 47) return nullptr;
    return reinterpret_cast<CTrackCell*>(Base() + 0x28 * (y * 128 + x));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}

void StartFlashingObject(tag_sRideAnimSet* set) { ev("StartFlashingObject", {lab(set)}); }
void StopFlashingObject(tag_sRideAnimSet* set) { ev("StopFlashingObject", {lab(set)}); }

extern "C" {

uint64_t tpw_fb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"sets", &gRideAnimSets[0]},
        {"E0CC", &gLLCCE0CC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_fb_reset() { gJustCalls = 0; }

int32_t tpw_fb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mNeighbours));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet*));
    return n;
}

uint32_t tpw_fb_fix(uint64_t ride, int32_t a, int32_t b, int32_t c) {
    return LLCR_FixBridges(reinterpret_cast<CObject*>(static_cast<uintptr_t>(ride)), a != 0, b != 0, c != 0);
}

}  // extern "C"
