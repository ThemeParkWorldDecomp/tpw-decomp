// Test library for GetActualBraceParams and UpdatePylonStackStructures (unit v5278; libtpw_unit_GetActualBraceParams.dylib,
// tests/test_GetActualBraceParams.py), main app: src/TPCoaster_loaders_GetActualBraceParams.cpp
//
// GetAlignmentParamsSlow is a fake that records what the real one reads. The globals gPylonStackSkip and gCoasterCurrentData are
// defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "m3d2.h"

using namespace calllog;

uint32_t gPylonStackSkip;
void* gCoasterCurrentData;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

void GetAlignmentParamsSlow(sCoasterTrackParams* params, s3DPoint* point, float f, sCoasterOrientation* orientation, int32_t a, float* out) {
    (void)orientation;
    uint32_t first = *reinterpret_cast<uint32_t*>(params);
    ev("GetAlignmentParamsSlow", {first, CanonBits(point->x), CanonBits(point->y), CanonBits(point->z), CanonBits(f), a,
                                  static_cast<int64_t>(reinterpret_cast<uintptr_t>(out))});
}

extern "C" {

uint64_t tpw_gabp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"skip", &gPylonStackSkip},
        {"data", &gCoasterCurrentData},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gabp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gabp_layout(int64_t* out) {
    int32_t n = 0;
    // order: the test's HOST_KEYS
    out[n++] = offsetof(sBraceWalkNode, mFlags);
    out[n++] = offsetof(sBraceWalkNode, mNext);
    out[n++] = offsetof(sBraceWalkNode, mF6C);
    out[n++] = offsetof(sBraceWalkNode, mList);
    out[n++] = offsetof(sBraceWalkList, mRecords);
    out[n++] = offsetof(sBraceWalkList, mEnd);
    out[n++] = offsetof(sBraceWalkData, mHead);
    out[n++] = offsetof(sPylonStackHead, mPylons);
    out[n++] = offsetof(sStackPylon, mF74);
    out[n++] = offsetof(sStackPylon, mArea78);
    out[n++] = offsetof(sStackPylon, mArea84);
    out[n++] = offsetof(sStackPylon, mF90);
    out[n++] = offsetof(sStackPylon, mCtl);
    out[n++] = offsetof(sStackCtl, mQ);
    out[n++] = offsetof(sStackQ, mArea30);
    out[n++] = offsetof(sStackQ, mArea44);
    out[n++] = sizeof(sBraceWalkRecord);
    out[n++] = offsetof(sBraceWalkRecord, mPoint);
    out[n++] = offsetof(sBraceWalkRecord, mF0C);
    out[n++] = offsetof(sBraceWalkNode, mFlags);
    return n;
}

void tpw_gabp_get() { GetActualBraceParams(); }

void tpw_gabp_upd(uint64_t pylon) { UpdatePylonStackStructures(nullptr, reinterpret_cast<tag_sPylon*>(static_cast<uintptr_t>(pylon))); }

}  // extern "C"
