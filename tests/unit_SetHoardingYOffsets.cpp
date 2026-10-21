// Test library for the ride hoardings' height offsets (unit v5674; libtpw_unit_SetHoardingYOffsets.dylib, tests/test_SetHoardingYOffsets.py), main app:
//   src/game/park/TPHoardings.cpp   SetHoardingYOffsets, GetRotatedHFieldCoord
//
// The callee outside the unit is FloatToCell (0x10237cd8), a fake that records its argument and answers from the test's queue.
// gHeightfieldHeader (another file's global) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "TPEngine.h"
#include "TPCoaster.h"
#include "m3d2.h"

using namespace calllog;

tag_sM3D2Header* gHeightfieldHeader;

// ---- the fake: the same event as the PowerPC stub (the double's bits)
int32_t FloatToCell(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    if (value != value) bits = 0x7ff8000000000000LL;
    ev("Cell", {bits});
    return static_cast<int32_t>(ret("Cell"));
}

extern "C" {

uint64_t tpw_sh_addr(const char* name) {
    if (std::strcmp(name, "gHeightfieldHeader") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gHeightfieldHeader));
    return 0;
}

uint64_t tpw_sh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offset a Ptr32 field of the host holds for a block address.
uint32_t tpw_sh_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_sh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));            // 0
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));            // 1
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHoardingGeometry)); // 2
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mDynamicMeshSet));   // 3
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC0));              // 4
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC4));              // 5
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));                  // 6
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF10));                     // 7
    out[n++] = static_cast<int64_t>(offsetof(sHoardingGeometry, mBlock));           // 8
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mVertexSets));          // 9
    out[n++] = static_cast<int64_t>(sizeof(sVertexPosnSet));                        // 10
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));               // 11
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));            // 12
    out[n++] = static_cast<int64_t>(offsetof(sHField, mF18));                       // 13
    out[n++] = static_cast<int64_t>(offsetof(sHField, mHeights));                   // 14
    return n;
}

void tpw_sh_set(uint64_t set) { SetHoardingYOffsets(reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set))); }

void tpw_sh_rot(int32_t angle, float a1, float a2, float a3, float a4, float f5, float f6, uint64_t outX, uint64_t outZ) {
    GetRotatedHFieldCoord(angle, a1, a2, a3, a4, f5, f6, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(outX)),
                          reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(outZ)));
}

}  // extern "C"
