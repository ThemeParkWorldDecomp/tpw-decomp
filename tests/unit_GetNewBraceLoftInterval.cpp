// Test library for the loft of a new brace and the link level of a pylon (unit v5053; libtpw_unit_GetNewBraceLoftInterval.dylib,
// tests/test_GetNewBraceLoftInterval.py), main app:
//   src/GetNewBraceLoftInterval.cpp   GetNewBraceLoftInterval, DetermineLinkLevel
//
// The callees outside the unit (CTRACE, CalculateBraceHeight, DistanceBetweenPoints, GetIntervalForHeight, SetLoftPylon) are fakes that
// record what the real ones read and answer from the test's queues; the globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterInputFlags;
tag_sRideAnimSet* gRideAnimSets[8];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float BitsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t WordAt(const void* base, size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const uint8_t*>(base) + offset, sizeof(word));
    return word;
}

const char* const kTexts[] = {
    "Loop is linking ontop of pylon at level %d with height %.3f\n",
    "More than enough room inside pylon at level %d with height %.3f\n",
    "Can just squeeze inside pylon at level %d with height %.3f - not returning yet though\n",
    "Sticking brace on top at level %d with height %.3f\n",
    "Going to squeeze inside pylon at level %d with height %.3f\n",
    "Can't fit anywhere without moving track, setting valid at top, level %d with height %.3f\n",
};

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t number = va_arg(args, int32_t);
    double value = va_arg(args, double);
    va_end(args);
    int64_t index = -1;
    for (int i = 0; i < 6; i++)
        if (std::strcmp(kTexts[i], format) == 0) index = i;
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    ev("CTRACE", {static_cast<int64_t>(level), index, number, static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xffffffffu)});
}

float CalculateBraceHeight(tag_sTrackBrace* brace) {
    ev("CalcH", {static_cast<int64_t>(WordAt(brace, 0)), static_cast<int64_t>(WordAt(brace, 4))});
    return BitsFloat(static_cast<uint32_t>(ret("CalcH")));
}

float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    ev("Dist", {CanonBits(a->x), CanonBits(a->y), CanonBits(a->z), CanonBits(b->x), CanonBits(b->y), CanonBits(b->z)});
    return BitsFloat(static_cast<uint32_t>(ret("Dist")));
}

float GetIntervalForHeight(float height, sPylonControl* control) {
    ev("Interval", {CanonBits(height), control ? static_cast<int64_t>(control->mF0C) : 0});
    return BitsFloat(static_cast<uint32_t>(ret("Interval")));
}

void SetLoftPylon(float value, uint32_t id, int32_t a, int32_t b) {
    ev("SetLoft", {CanonBits(value), static_cast<int64_t>(id), a, b});
}

extern "C" {

uint64_t tpw_lay_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},
        {"flags", &gCoasterInputFlags},
        {"sets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lay_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lay_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(sLinkPylon, mControl);
    out[n++] = offsetof(sLinkPylon, mX);
    out[n++] = offsetof(sLinkPylon, mY);
    out[n++] = offsetof(sLinkPylon, mZ);
    out[n++] = offsetof(sLinkPylon, mOther);
    out[n++] = offsetof(sLinkPylon, mCount);
    out[n++] = offsetof(sLinkPylon, mBraces);
    out[n++] = offsetof(sPylonControl, mF0C);
    out[n++] = offsetof(sPylonControl, mF2C);
    out[n++] = offsetof(sPylonControl, mF68);
    out[n++] = offsetof(sPylonControl, mF6C);
    out[n++] = offsetof(sPylonControl, mFC4);
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = offsetof(tag_sTrackBrace, mWord70);
    out[n++] = sizeof(tag_sTrackBrace);
    out[n++] = offsetof(sCoasterPylon, mFlags);
    out[n++] = offsetof(sCoasterPylon, mControl);
    out[n++] = sizeof(sCoasterPylon);
    out[n++] = offsetof(tag_sRideAnimSet, mHeads);
    out[n++] = offsetof(tag_sMesh, mF28);
    out[n++] = offsetof(tag_sCoasterInstance, mF118);
    out[n++] = sizeof(sPylonControl);
    out[n++] = sizeof(tag_sRideAnimSet);
    out[n++] = sizeof(tag_sMesh);
    out[n++] = sizeof(tag_sCoasterInstance);
    return n;
}

float tpw_lay_gnbli(uint64_t pylon, uint64_t control, uint64_t brace, uint64_t other, int32_t a, int32_t b, uint64_t out) {
    return GetNewBraceLoftInterval(reinterpret_cast<sLinkPylon*>(pylon), reinterpret_cast<sPylonControl*>(control),
                                   reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<tag_sTrackBrace*>(other), a, b,
                                   reinterpret_cast<float*>(out));
}

uint32_t tpw_lay_dll(float height, uint64_t pylon, uint64_t level, uint64_t out, int32_t quiet) {
    return static_cast<uint32_t>(DetermineLinkLevel(height, reinterpret_cast<sLinkPylon*>(pylon), reinterpret_cast<uint32_t*>(level),
                                                    reinterpret_cast<float*>(out), quiet));
}

}  // extern "C"
