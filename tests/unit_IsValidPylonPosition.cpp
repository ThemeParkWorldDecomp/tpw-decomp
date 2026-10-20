// Test library for IsValidPylonPosition (unit v5551; libtpw_unit_IsValidPylonPosition.dylib, tests/test_IsValidPylonPosition.py), main app:
//   src/TPCoaster_instances_IsValidPylonPosition.cpp   IsValidPylonPosition and the clamp globals gPylonClampMax / gPylonClampMin
//
// The callee outside the unit is XZAngleBetweenPylons: a fake that records its six arguments and answers the float bits of the test's queue
// (sqrt is the host's, the PowerPC side has the same one from install_std_stubs). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
tag_sCoasterInstance* gCoasterEditInstance;
tag_sCoasterInstance* gCoasterInstances;
tag_sTrackBrace* gPylonMoveBrace;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
tag_sM3D2Header* gHeightfieldHeader;
sHField* gHeightfieldCopy;

// ---- the fake: one event, the answer from the queue "XZ" as float bits
float XZAngleBetweenPylons(uint32_t x1, uint32_t z1, uint32_t x2, uint32_t z2, uint32_t x3, uint32_t z3) {
    ev("XZ", {static_cast<int64_t>(x1), static_cast<int64_t>(z1), static_cast<int64_t>(x2), static_cast<int64_t>(z2),
              static_cast<int64_t>(x3), static_cast<int64_t>(z3)});
    uint32_t bits = static_cast<uint32_t>(ret("XZ"));
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

extern "C" {

uint64_t tpw_ivp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance}, {"insts", &gCoasterInstances}, {"mv", &gPylonMoveBrace},
        {"w1790", &gCoasterWord1790},    {"inflags", &gCoasterInputFlags}, {"hhdr", &gHeightfieldHeader},
        {"hcopy", &gHeightfieldCopy},    {"cmax", &gPylonClampMax},        {"cmin", &gPylonClampMin},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ivp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// What a Ptr32 holds for an address: the offset from the arena base.
uint64_t tpw_ivp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_ivp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sCoasterInstance, mNext);
    out[n++] = offsetof(tag_sCoasterInstance, mBlock3C);
    out[n++] = offsetof(tag_sCoasterInstance, mType);
    out[n++] = offsetof(tag_sCoasterInstance, mPylons);
    out[n++] = offsetof(tag_sCoasterInstance, mLastBrace);
    out[n++] = offsetof(tag_sCoasterInstance, mBraces);
    out[n++] = offsetof(tag_sCoasterInstance, mPreviewBrace);
    out[n++] = offsetof(tag_sCoasterInstance, mBrace64);
    out[n++] = sizeof(tag_sCoasterInstance);
    out[n++] = offsetof(tag_sTrackBrace, mNumber);
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mPrev);
    out[n++] = offsetof(tag_sTrackBrace, mNext);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = sizeof(tag_sTrackBrace);
    out[n++] = offsetof(sPylonRecord, mFlags);
    out[n++] = offsetof(sPylonRecord, mIndex);
    out[n++] = offsetof(sPylonRecord, mCellX);
    out[n++] = offsetof(sPylonRecord, mCellZ);
    out[n++] = offsetof(sPylonRecord, mInstance);
    out[n++] = offsetof(sPylonRecord, mControl);
    out[n++] = offsetof(sPylonRecord, mNext);
    out[n++] = offsetof(sPylonRecord, mCount);
    out[n++] = offsetof(sPylonRecord, mBraces);
    out[n++] = sizeof(sPylonRecord);
    out[n++] = offsetof(tag_sCoasterType, mPylonsA);
    out[n++] = offsetof(tag_sCoasterType, mPylonsB);
    out[n++] = sizeof(tag_sCoasterType);
    out[n++] = offsetof(sPylonControl, mF9C);
    out[n++] = offsetof(sPylonControl, mFA0);
    out[n++] = offsetof(sPylonControl, mFA4);
    out[n++] = offsetof(sPylonControl, mFA8);
    out[n++] = offsetof(sPylonControl, mAngles);
    out[n++] = offsetof(sPylonControl, mFCC);
    return n;
}

int32_t tpw_ivp_call(uint32_t x, uint32_t z, uint64_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    return IsValidPylonPosition(x, z, reinterpret_cast<void*>(static_cast<uintptr_t>(a)), b, c, d, e);
}

}  // extern "C"
