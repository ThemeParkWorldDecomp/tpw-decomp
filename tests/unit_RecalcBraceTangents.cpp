// Test library for RecalcBraceTangents (unit v5673; libtpw_unit_RecalcBraceTangents.dylib, tests/test_RecalcBraceTangents.py), main app:
//   src/game/rides/CoasterPylon.cpp   RecalcBraceTangents
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterCurrent;
uint32_t gCoasterWord7310;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float BitsToFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

float RetFloat(const char* key) { return BitsToFloat(static_cast<uint32_t>(ret(key))); }

uint32_t Fnv(const char* text) {
    uint32_t h = 2166136261u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint8_t>(*text)) * 16777619u;
    return h;
}

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<const char*>(&reinterpret_cast<const T*>(0x10000)->m) - reinterpret_cast<const char*>(0x10000))

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
float GetHeightForInterval(float interval, sPylonControl* pylon) {
    ev("GetHeightForInterval", {CanonBits(interval), lab(pylon)});
    return RetFloat("height");
}

void GetRequiredBraceTangents(tag_sTrackBrace* brace, sCoasterType* type, sCoasterTrackParams* params) {
    ev("GetRequiredBraceTangents", {lab(brace), lab(type), lab(params)});
}

void GetAlignmentParamsSlow(sCoasterTrackParams* params, s3DPoint* point, float f, sCoasterOrientation* orientation, int32_t a, float* out) {
    ev("GetAlignmentParamsSlow", {lab(params), CanonBits(point->x), CanonBits(point->y), CanonBits(point->z), CanonBits(f), a, lab(out)});
    orientation->mF04 = RetFloat("gap4");
    orientation->mF08 = RetFloat("gap8");
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    uint32_t number = va_arg(args, uint32_t);
    double first = va_arg(args, double);
    double second = va_arg(args, double);
    va_end(args);
    uint64_t a, b;
    std::memcpy(&a, &first, 8);
    std::memcpy(&b, &second, 8);
    ev("CTRACE", {level, Fnv(format), number, static_cast<int64_t>(a >> 32), static_cast<int64_t>(a & 0xffffffffu), static_cast<int64_t>(b >> 32),
                  static_cast<int64_t>(b & 0xffffffffu)});
}

float GetBankAtBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, float f) {
    ev("GetBankAtBrace", {lab(instance), lab(brace), CanonBits(f)});
    return RetFloat("bank");
}

float GetCarWantedSpeedAtHeight(sCoasterType* type, float a, float b, float c, float d) {
    ev("GetCarWantedSpeedAtHeight", {lab(type), CanonBits(a), CanonBits(b), CanonBits(c), CanonBits(d)});
    return RetFloat("speed");
}

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("SetupBraceAnimFrameData", {lab(type), lab(brace)}); }

void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) { ev("AnimateRideSet", {lab(set), a, flags}); }

void GetActualBraceTrackPositions(tag_sRideAnimSet* set, tag_sTrackBrace* brace) { ev("GetActualBraceTrackPositions", {lab(set), lab(brace)}); }

void GetActualBraceTangents(tag_sTrackBrace* brace, tag_sRideAnimSet* set) { ev("GetActualBraceTangents", {lab(brace), lab(set)}); }

extern "C" {

uint64_t tpw_rbt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterCurrentData", &gCoasterCurrentData},
        {"gCoasterCurrent", &gCoasterCurrent},
        {"gCoasterWord7310", &gCoasterWord7310},
        {"gRideAnimSets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rbt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets, in the order of the module's field lists (brace, node, data, control, instance).
int32_t tpw_rbt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(tag_sTrackBrace, mFlags);
    out[n++] = OFF(tag_sTrackBrace, mPrev);
    out[n++] = OFF(tag_sTrackBrace, mF0C) + 0x34 - 0x0c;
    out[n++] = OFF(tag_sTrackBrace, mX4C);
    out[n++] = OFF(tag_sTrackBrace, mL50);
    out[n++] = OFF(tag_sTrackBrace, mZ54);
    out[n++] = OFF(tag_sTrackBrace, mF6C);
    out[n++] = OFF(tag_sTrackBrace, mWord70);
    out[n++] = OFF(tag_sTrackBrace, mFD4);
    out[n++] = OFF(tag_sTrackBrace, mFD8);
    out[n++] = OFF(tag_sTrackBrace, mFDC);
    out[n++] = OFF(tag_sTrackBrace, mAngle);
    out[n++] = OFF(tag_sTrackBrace, mFE8w);
    out[n++] = OFF(tag_sTrackBrace, mFEC);
    out[n++] = OFF(tag_sTrackBrace, mFF0);
    out[n++] = OFF(tag_sTrackBrace, mFF4);
    out[n++] = OFF(tag_sTrackBrace, mData);
    out[n++] = OFF(tag_sTrackBrace, mF00) + 4;
    out[n++] = OFF(tag_sTrackBrace, mNumber);
    out[n++] = OFF(sCoasterPylon, mNext);
    out[n++] = OFF(sCoasterPylon, mCount);
    out[n++] = OFF(sCoasterPylon, mBraces);
    out[n++] = OFF(sTrackBraceData, mFlags);
    out[n++] = OFF(sTrackBraceData, mF18Table);
    out[n++] = OFF(sTrackBraceData, mF1C);
    out[n++] = OFF(sTrackBraceData, mOwner);
    out[n++] = OFF(sPylonControl, mF2C);
    out[n++] = OFF(sPylonControl, mFC4);
    out[n++] = OFF(tag_sCoasterInstance, mType);
    out[n++] = OFF(tag_sCoasterInstance, mPylons);
    out[n++] = OFF(tag_sCoasterInstance, mLastBrace);
    return n;
}

void tpw_rbt_run() { RecalcBraceTangents(); }

}  // extern "C"
