// Test library for LLCC_BreakTrack and LLCC_GetTrackMeshAt (unit v5687; libtpw_unit_LLCC_BreakTrack.dylib, tests/test_LLCC_BreakTrack.py), main app:
//   src/game/park/TrackCell.cpp
//
// The cell methods, OPPOSITE, CMap::IsXYValid and CTRACE are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "Debug.h"

using namespace calllog;

int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
int32_t gLLCCE034;
int32_t gLLCCE038;
uint16_t gLLCCE380;
CWorld* gGameWorld;

namespace {
int64_t Idx(const void* p) {
    return (static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(gGameWorld)) - 0x1102d8) / 0x28;
}
}  // namespace

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Idx(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Idx(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {Idx(this), mask}); }
void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModifyNeighbours", {Idx(this), mask}); }
int16_t CCellBase::GetOverlapCounter() const {
    ev("GetOverlapCounter", {Idx(this)});
    return static_cast<int16_t>(ret("GetOverlapCounter"));
}
void CCellBase::DecOverlapCounter() { ev("DecOverlapCounter", {Idx(this)}); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {Idx(this)}); }
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("GetNeighbouringCell", {Idx(this), direction, offset});
    int64_t v = ret("GetNeighbouringCell");
    if (v == 0) return nullptr;
    return &gGameWorld->mTrackCells[v - 1];
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>((direction << 4) | (direction >> 4));
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

extern "C" {

uint64_t tpw_llcb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gw", &gGameWorld}, {"fbx", &gLLCCFirstBlockX}, {"fby", &gLLCCFirstBlockY},
        {"e034", &gLLCCE034}, {"e038", &gLLCCE038}, {"e380", &gLLCCE380},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llcb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llcb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    return n;
}

uint64_t tpw_llcb_break(int32_t x, int32_t y, uint32_t dir) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCC_BreakTrack(x, y, static_cast<uint8_t>(dir))));
}
uint32_t tpw_llcb_mesh(int32_t x, int32_t y) { return LLCC_GetTrackMeshAt(x, y); }

}  // extern "C"
