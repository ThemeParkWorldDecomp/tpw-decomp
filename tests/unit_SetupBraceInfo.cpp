// Test library for the brace info and brace/pylon link functions (unit v5731; libtpw_unit_SetupBraceInfo.dylib, tests/test_SetupBraceInfo.py), main app:
//   src/GetNewBraceLoftInterval_SetupBraceInfo.cpp   SetupBraceInfo, DestroyBraceInfo, LinkPylon, LinkBrace, UnLinkBrace
//
// The callees outside the unit (MReleaseAllocF, MReleaseFreeF, FindDataNodeIndexByID, GetRideRequiredYOffset, CorrectBraceNumbering) are
// recording fakes; gRideTypes (src/RideTypes.cpp) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

// ---- globals of other files the unit reads
tag_sRideAnimSet* gRideTypes[330];

// ---- the fakes
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {static_cast<int64_t>(size), static_cast<int64_t>(flags)});
    return tpw::ArenaAllocZeroed(size ? size : 4);
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("Find", {lab(set), static_cast<int64_t>(mask), static_cast<int64_t>(id)});
    return static_cast<uint32_t>(ret("Find"));
}

float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation) {
    ev("Yoff", {static_cast<int64_t>(static_cast<uint32_t>(x)), static_cast<int64_t>(static_cast<uint32_t>(z)),
                static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(a))), static_cast<int64_t>(static_cast<uint32_t>(w)),
                static_cast<int64_t>(static_cast<uint32_t>(h)), static_cast<int64_t>(flags), static_cast<int64_t>(static_cast<uint32_t>(rotation))});
    uint32_t bits = static_cast<uint32_t>(ret("Yoff"));
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

void CorrectBraceNumbering(tag_sTrackBrace* brace) { ev("Correct", {lab(brace)}); }

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_sbi_addr(const char* name) {
    if (std::strcmp(name, "gRideTypes") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRideTypes));
    return 0;
}

uint64_t tpw_sbi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the fields the module writes by Mac offset, in the order of the module's FIELDS.
int32_t tpw_sbi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mNumber);
    out[n++] = offsetof(tag_sTrackBrace, mPrev);
    out[n++] = offsetof(tag_sTrackBrace, mNext);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = offsetof(sTrackBraceData, mFlags);
    out[n++] = offsetof(sCoasterTrackParams, mPylonCount);
    out[n++] = offsetof(sCoasterTrackParams, mBraceCount);
    out[n++] = offsetof(sCoasterTrackParams, mPylonTail);
    out[n++] = offsetof(sCoasterTrackParams, mBraceHead);
    out[n++] = offsetof(sCoasterTrackParams, mBraceTail);
    out[n++] = offsetof(sPylonRecord, mNumber);
    out[n++] = offsetof(sPylonRecord, mFlags);
    out[n++] = offsetof(sPylonRecord, mIndex);
    out[n++] = offsetof(sPylonRecord, mCellX);
    out[n++] = offsetof(sPylonRecord, mCellZ);
    out[n++] = offsetof(sPylonRecord, mX);
    out[n++] = offsetof(sPylonRecord, mY);
    out[n++] = offsetof(sPylonRecord, mZ);
    out[n++] = offsetof(sPylonRecord, mNext);
    out[n++] = offsetof(sPylonRecord, mPrev);
    out[n++] = offsetof(sBraceInfo, mCount);
    out[n++] = offsetof(sBraceInfo, mBlockA);
    out[n++] = offsetof(sBraceInfo, mBlockB);
    out[n++] = offsetof(sBraceControl, mCount);
    out[n++] = offsetof(sBraceControl, mBase);
    out[n++] = offsetof(sPylonControl, mRideType);
    return n;
}

void tpw_sbi_setup(uint64_t brace, uint64_t info, uint64_t control, uint64_t pylon) {
    SetupBraceInfo(reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<sBraceInfo*>(info), reinterpret_cast<sBraceControl*>(control),
                   reinterpret_cast<sPylonControl*>(pylon));
}

void tpw_sbi_destroy(uint64_t brace, uint64_t info) {
    DestroyBraceInfo(reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<sBraceInfo*>(info));
}

void tpw_sbi_linkpylon(uint64_t pylon, uint64_t params, uint32_t x, uint32_t z, uint32_t index) {
    LinkPylon(reinterpret_cast<sPylonRecord*>(pylon), reinterpret_cast<sCoasterTrackParams*>(params), x, z, index);
}

void tpw_sbi_linkbrace(uint64_t brace, uint64_t params, int32_t valid) {
    LinkBrace(reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<sCoasterTrackParams*>(params), valid);
}

void tpw_sbi_unlink(uint64_t brace, uint64_t params) {
    UnLinkBrace(reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<sCoasterTrackParams*>(params));
}

}  // extern "C"
