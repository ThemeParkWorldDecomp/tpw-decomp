// Test library for the pylon brace functions (unit v5667; libtpw_unit_RestorePylonDetails.dylib, tests/test_RestorePylonDetails.py), main app:
//   src/GetNewBraceLoftInterval_RestorePylonDetails.cpp   CalculateBraceHeight, RestorePylonDetails, SquishPylonBracesIfRequired, InsertBraceIntoPylon
//
// The callees outside the unit (GetHeightForInterval, GetIntervalForHeight, SetupBraceAnimFrameData, UpdatePylonStackMeshes) are recording
// fakes that answer from the test's queues. The globals of other files the unit reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

// ---- globals of other files (the program's definitions are in files this library does not compile)
tag_sCoasterInstance* gCoasterEditInstance = nullptr;
uint32_t gPylonStackSkip = 0;
uint32_t gLinkSavedFlag = 0;
sPylonRecord gLinkSavedPylon;
tag_sTrackBrace gLinkSavedBraces[5];
float gLinkSavedHeights[5];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

alignas(16) uint8_t gPool[19 * 0x200];

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
float GetHeightForInterval(float interval, sPylonControl* control) {
    ev("GetHeightForInterval", {CanonBits(interval), lab(control)});
    return FromBits(ret("GetHeightForInterval"));
}

float GetIntervalForHeight(float height, sPylonControl* control) {
    ev("GetIntervalForHeight", {CanonBits(height), lab(control)});
    return FromBits(ret("GetIntervalForHeight"));
}

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("Setup", {lab(type), lab(brace)}); }

void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("Update", {lab(control), lab(pylon), a, b});
}

extern "C" {

uint64_t tpw_rpd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"editinst", &gCoasterEditInstance}, {"skip", &gPylonStackSkip},       {"savedflag", &gLinkSavedFlag},
        {"savedpylon", &gLinkSavedPylon},    {"savedbraces", gLinkSavedBraces}, {"savedheights", gLinkSavedHeights},
        {"pool", gPool},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The host offsets, in the order of the module's KB, KP, KC, KI lists.
int32_t tpw_rpd_layout(int64_t* out) {
    int32_t n = 0;
#define O(type, member) out[n++] = static_cast<int64_t>(offsetof(type, member))
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF00) + 4);   // num
    O(tag_sTrackBrace, mFlags);
    O(tag_sTrackBrace, mPrev);
    O(tag_sTrackBrace, mNext);
    O(tag_sTrackBrace, mF6C);
    O(tag_sTrackBrace, mF74);
    O(tag_sTrackBrace, mF90);
    O(tag_sTrackBrace, mF94w);
    O(tag_sTrackBrace, mW98);
    O(tag_sTrackBrace, mW9C);
    O(tag_sTrackBrace, mWA0);
    O(tag_sTrackBrace, mWA4);
    O(tag_sTrackBrace, mWA8);
    O(tag_sTrackBrace, mWAC);
    O(tag_sTrackBrace, mFD4);
    O(tag_sTrackBrace, mFD8);
    O(tag_sTrackBrace, mFDC);
    O(tag_sTrackBrace, mAngle);
    O(tag_sTrackBrace, mFE4);
    O(tag_sTrackBrace, mFE8w);
    O(tag_sTrackBrace, mFEC);
    O(tag_sTrackBrace, mFF0);
    O(tag_sTrackBrace, mFF4);
    O(tag_sTrackBrace, mFF8);
    O(tag_sTrackBrace, mSample);
    O(tag_sTrackBrace, mData);
    O(tag_sTrackBrace, mCount78);
    O(tag_sTrackBrace, mCount84);
    O(tag_sTrackBrace, mF0C);
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    O(sPylonRecord, mNumber);
    O(sPylonRecord, mFlags);
    O(sPylonRecord, mIndex);
    O(sPylonRecord, mCellX);
    O(sPylonRecord, mCellZ);
    O(sPylonRecord, mInstance);
    O(sPylonRecord, mControl);
    O(sPylonRecord, mX);
    O(sPylonRecord, mY);
    O(sPylonRecord, mZ);
    O(sPylonRecord, mNext);
    O(sPylonRecord, mPrev);
    O(sPylonRecord, mCount);
    O(sPylonRecord, mBraces);
    out[n++] = static_cast<int64_t>(sizeof(sPylonRecord));
    O(sPylonControl, mFlags);
    O(sPylonControl, mF68);
    O(sPylonControl, mF6C);
    O(sPylonControl, mFC4);
    O(sPylonControl, mF30);
    O(sPylonControl, mF44);
    O(tag_sCoasterInstance, mType);
#undef O
    return n;
}

static float AsFloat(uint32_t bits) { return FromBits(bits); }

uint32_t tpw_rpd_cbh(uint64_t brace) {
    float value = CalculateBraceHeight(reinterpret_cast<tag_sTrackBrace*>(brace));
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void tpw_rpd_restore(uint64_t instance, uint64_t pylon) {
    RestorePylonDetails(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sPylon*>(pylon));
}

int32_t tpw_rpd_squish(uint64_t instance, uint64_t control, uint64_t pylon) {
    return SquishPylonBracesIfRequired(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sPylonControl*>(control),
                                       reinterpret_cast<sLinkPylon*>(pylon));
}

void tpw_rpd_insert(uint64_t instance, uint64_t brace, uint64_t pylon, uint32_t level, uint32_t heightBits, uint64_t out, int32_t a, int32_t b,
                    int32_t c, int32_t d) {
    InsertBraceIntoPylon(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sTrackBrace*>(brace),
                         reinterpret_cast<sLinkPylon*>(pylon), level, AsFloat(heightBits), reinterpret_cast<float*>(out), a, b, c, d);
}

}  // extern "C"
