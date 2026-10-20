// Test library for the ride highlight and flash functions and the shape edge marker (unit v5915;
// libtpw_unit_DecideRideShapeDeformEdges.dylib, tests/test_DecideRideShapeDeformEdges.py), main app:
//   src/Debug_overlays_DecideRideShapeDeformEdges.cpp   UnhighlightRide, StartFlashingObject, StopFlashingObject, DecideRideShapeDeformEdges
//
// MReleaseAllocF answers the block the test chose; MReleaseFreeF logs whether it got the block the test expected; HighlightRide logs.
// The globals of other files the unit uses are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "LevelSystems.h"
#include "TPEngine.h"
#include "m3d2.h"

using namespace calllog;

tag_sRideAnimSet* gHighlightedRide = nullptr;
tag_sRideAnimSet* gFlashObjects = nullptr;
void* gHighlightNodeColours = nullptr;
uint32_t gSkirtCellFlags[20][20];
uint32_t gSkirtRows = 0;
uint32_t gSkirtColumns = 0;

namespace {
void* gNewBlock = nullptr;
void* gExpectFree = nullptr;
}  // namespace

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    return gNewBlock;
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {block == gExpectFree ? 1 : 0});
    return nullptr;
}

void HighlightRide() { ev("HighlightRide", {}); }

extern "C" {

uint64_t tpw_dr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hl", &gHighlightedRide},
        {"flash", &gFlashObjects},
        {"colours", &gHighlightNodeColours},
        {"skirt", gSkirtCellFlags},
        {"rows", &gSkirtRows},
        {"cols", &gSkirtColumns},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_dr_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_dr_cfg(uint64_t newBlock, uint64_t expectFree) {
    gNewBlock = reinterpret_cast<void*>(static_cast<uintptr_t>(newBlock));
    gExpectFree = reinterpret_cast<void*>(static_cast<uintptr_t>(expectFree));
}

// The Mac's offsets are checked by the module; these are the host-only ones (the native pointers).
int32_t tpw_dr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mPointCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mPoints));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mSelf));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF5C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF5E));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mPoints));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(offsetof(RideShape, mCells));
    out[n++] = static_cast<int64_t>(sizeof(RideShapeCell));
    // host-only
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFD0));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    return n;
}

void tpw_dr_unhighlight() { UnhighlightRide(); }
void tpw_dr_start(uint64_t set) { StartFlashingObject(reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set))); }
void tpw_dr_stop(uint64_t set) { StopFlashingObject(reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set))); }
void tpw_dr_decide(uint64_t shape, int32_t width, int32_t height) {
    DecideRideShapeDeformEdges(reinterpret_cast<RideShape*>(static_cast<uintptr_t>(shape)), width, height);
}

}  // extern "C"
