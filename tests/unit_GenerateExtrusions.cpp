// Test library for GenerateExtrusions (unit v5557; libtpw_unit_GenerateExtrusions.dylib, tests/test_GenerateExtrusions.py), main app:
//   src/game/rides/CoasterExtrude.cpp   GenerateExtrusions and gExtrudeBlendMin/Max, gExtrudeParamsBuf
//
// The callees outside the unit (SetInitialCoasterInterpParams, IsEndOfSampleBuffer, InterpolateCoasterSpline, GetCrossSectionFromParams, fmod,
// modf) are fakes that record the arguments the real ones read and answer from the test's queues. The globals of other files the unit
// reads are defined here, zero at start like the Mac's bss.
#include <dlfcn.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sCoasterInstance* gCoasterCurrent;
void* gCoasterCurrentData;
sExtrusionParams gExtrudeParamsStatic[2];

namespace {

uint64_t gRecs[8];   // the records (by index) the case allocated; GetCrossSectionFromParams answers one of them

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FloatOfBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &b, sizeof(value));
    return value;
}

uint64_t DoubleBits(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

}  // namespace

// ---- the fakes
void SetInitialCoasterInterpParams(sCoasterTrackParams* params) { ev("SIP", {lab(params)}); }

int32_t IsEndOfSampleBuffer(tag_sCoasterInstance* instance, float position) {
    ev("IEOS", {lab(instance), CanonBits(position)});
    return static_cast<int32_t>(ret("IEOS"));
}

void InterpolateCoasterSpline(tag_sCoasterInstance* instance, float position, s3DPoint* point, sCoasterOrientation* orient) {
    ev("ICS", {lab(instance), CanonBits(position), lab(orient)});
    point->x = 0.0f;
    point->y = 0.0f;
    point->z = 0.0f;
    orient->mF04 = FloatOfBits(ret("ICS_a"));
    orient->mF08 = FloatOfBits(ret("ICS_b"));
    orient->mF0C = FloatOfBits(ret("ICS_c"));
}

int32_t GetCrossSectionFromParams(sCoasterType* type, float unused, sCoasterOrientation* orientation, float angle) {
    ev("GCS", {lab(type), CanonBits(unused), lab(orientation), CanonBits(angle)});
    int64_t index = ret("GCS");
    uint64_t record = gRecs[index & 7];   // the answer is an arena offset, like the type table's words
    return record != 0 ? static_cast<int32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(record))) : 0;
}

extern "C" {

double fmod(double x, double y) {
    uint64_t a = DoubleBits(x), b = DoubleBits(y);
    ev("fmod", {static_cast<int64_t>(a >> 32), static_cast<int64_t>(a & 0xffffffffu), static_cast<int64_t>(b >> 32),
                static_cast<int64_t>(b & 0xffffffffu)});
    using Fn = double (*)(double, double);
    static Fn real = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, "fmod"));
    return real(x, y);
}

double modf(double x, double* whole) {
    uint64_t a = DoubleBits(x);
    ev("modf", {static_cast<int64_t>(a >> 32), static_cast<int64_t>(a & 0xffffffffu)});
    double w = std::trunc(x);
    *whole = w;
    return x - w;
}

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ge_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"cur", &gCoasterCurrent},       {"curdata", &gCoasterCurrentData}, {"g0", &gExtrudeBlendMin}, {"g1", &gExtrudeBlendMax},
        {"static", gExtrudeParamsStatic}, {"buf", gExtrudeParamsBuf},       {"recs", gRecs},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ge_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ge_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_ge_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mSamples));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(sizeof(sTrackSample));
    out[n++] = static_cast<int64_t>(offsetof(sTrackSample, mPiece));
    out[n++] = static_cast<int64_t>(offsetof(tag_sBezierControl, mF08));
    out[n++] = static_cast<int64_t>(offsetof(tag_sBezierControl, mNext));
    out[n++] = static_cast<int64_t>(sizeof(tag_sBezierControl));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mF14));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterRecord));
    out[n++] = static_cast<int64_t>(sizeof(sExtrusionParams));
    out[n++] = static_cast<int64_t>(offsetof(sExtrusionParams, mSection));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterType, mTable));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterType, mCount));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterType));
    out[n++] = static_cast<int64_t>(kCoasterTypeSectionB);
    out[n++] = static_cast<int64_t>(kCoasterTypeSectionA);
    out[n++] = 0x74;   // the flags word
    out[n++] = 0x2c;   // a table entry
    out[n++] = 0;
    return n;
}

void tpw_ge_run() { GenerateExtrusions(); }

}  // extern "C"
