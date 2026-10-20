// Test library for the ride commands (unit v5220; libtpw_unit_BMP_FixRide.dylib, tests/test_BMP_FixRide.py), main app:
//   src/BMP_FixRide.cpp   BMP_HaltRide, BMP_BreakRide, BMP_FixRide, BMP_BeginRide
//
// The callees outside the unit (TRACE, PAR_KillEmitter, PAR_InitEmitter, FlushDeferredAnim, SetCurRideAnim, SetControlType, BMP_DOCK,
// BMP_CloseRide) are recording fakes. Objects are named in the log by their index (ride in the ride block, bump in the bump block, set in
// gRideAnimSets). The globals gColList and gRideAnimSets of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

tabCOLLIST gColList;
tag_sRideAnimSet* gRideAnimSets[8];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

tagBUMPWORLD* World() { return reinterpret_cast<tagBUMPWORLD*>(&gColList); }

int64_t RideIndex(const void* ride) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(World()->rides.get());
    return (reinterpret_cast<const uint8_t*>(ride) - base) / 0xd0;
}

int64_t BumpIndex(const void* bump) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(World()->bumps.get());
    return (reinterpret_cast<const uint8_t*>(bump) - base) / 0xac;
}

int64_t SetIndex(const tag_sRideAnimSet* set) {
    for (int i = 0; i < 8; ++i)
        if (gRideAnimSets[i] == set) return i;
    return -1;
}

}  // namespace

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) { ev("FlushDeferredAnim", {SetIndex(set), flag}); }

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {SetIndex(set), a, anim, loop, CanonBits(speed), b});
}

void SetControlType(tagBUMPRIDE* ride, tagBUMP* bump) {
    const tagBUMPSLOT* slot = reinterpret_cast<const tagBUMPSLOT*>(bump);
    ev("SetControlType", {RideIndex(ride), BumpIndex(bump), slot->flags, slot->f88});
}

int32_t BMP_DOCK(tagBUMP* bump) {
    ev("BMP_DOCK", {BumpIndex(bump)});
    return static_cast<int32_t>(ret("BMP_DOCK"));
}

int32_t BMP_CloseRide(int32_t ride) {
    ev("BMP_CloseRide", {ride});
    return static_cast<int32_t>(ret("BMP_CloseRide"));
}

extern "C" {

uint64_t tpw_bump_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gColList));
    return 0;
}

uint64_t tpw_bump_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A Ptr32 word for an address, and back.
uint32_t tpw_bump_off(uint64_t address) { return address != 0 ? tpw::ArenaOffset(reinterpret_cast<void*>(address)) : 0; }
uint64_t tpw_bump_from_off(uint32_t offset) { return offset != 0 ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + offset)) : 0; }

// Builds gRideAnimSets[index]: the set, its mesh, the table and entries (all nodes null).
void tpw_bump_mkset(uint32_t index, uint32_t entries) {
    auto* set = static_cast<tag_sRideAnimSet*>(tpw::ArenaAllocZeroed(sizeof(tag_sRideAnimSet)));
    auto* mesh = static_cast<tag_sMesh*>(tpw::ArenaAllocZeroed(sizeof(tag_sMesh)));
    auto* table = static_cast<tagBumpHeadTable*>(tpw::ArenaAllocZeroed(sizeof(tagBumpHeadTable)));
    table->entries = static_cast<tagBumpHeadEntry*>(tpw::ArenaAllocZeroed(entries * sizeof(tagBumpHeadEntry)));
    mesh->mF28 = table;
    set->mHeads = mesh;
    gRideAnimSets[index] = set;
}

void tpw_bump_mknode(uint32_t index, uint32_t entry, uint32_t xbits, uint32_t ybits, uint32_t zbits) {
    auto* node = static_cast<tagBumpHeadNode*>(tpw::ArenaAllocZeroed(sizeof(tagBumpHeadNode)));
    std::memcpy(&node->x, &xbits, 4);
    std::memcpy(&node->y, &ybits, 4);
    std::memcpy(&node->z, &zbits, 4);
    static_cast<tagBumpHeadTable*>(gRideAnimSets[index]->mHeads->mF28)->entries[entry].node = node;
}

int32_t tpw_bump_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPSLOT, owner));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPWORLD, bumpCount));
    out[n++] = static_cast<int64_t>(offsetof(tagBumpHeadNode, z));
    return n;
}

void tpw_bump_halt(int32_t ride) { BMP_HaltRide(ride); }
void tpw_bump_break(int32_t ride) { BMP_BreakRide(ride); }
void tpw_bump_fix(int32_t ride) { BMP_FixRide(ride); }
void tpw_bump_begin(int32_t ride) { BMP_BeginRide(ride); }

}  // extern "C"
