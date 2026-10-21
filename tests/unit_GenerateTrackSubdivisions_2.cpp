// Test library for the track subdivisions, coaster speed, bank, end pylon flashing and the key action (unit v51008;
// libtpw_unit_GenerateTrackSubdivisions_2.dylib, tests/test_GenerateTrackSubdivisions_2.py), main app:
//   src/game/rides/CoasterExtrude.cpp, src/game/rides/CoasterTrack.cpp, src/game/rides/CoasterTrain.cpp   GenerateTrackSubdivisions, CoasterSetSpeed, GetBankAtBrace, FlashEndPylons, DecCoasterConst1
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "Debug.h"
#include "keytable_actions.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterEditInstance;
float gExtrudeTOffset;
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t InstId(const tag_sCoasterInstance* i) { return i->mTypeID; }
int64_t SetId(const tag_sRideAnimSet* s) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s->mHeads)); }

float BitsFloat(int64_t v) {
    uint32_t b = static_cast<uint32_t>(v);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

int64_t PointBits(const s3DPoint* p, int k) {
    const float* f = reinterpret_cast<const float*>(p);
    return CanonBits(f[k]);
}

}  // namespace

// ---- the fakes
void LayoutRecordCoasterSpeed(tag_sCoasterInstance* instance, uint32_t speed) { ev("LayoutRecordCoasterSpeed", {InstId(instance), speed}); }
void FlagRecalcAllCoasterCollisions(tag_sCoasterInstance* instance) { ev("FlagRecalcAllCoasterCollisions", {InstId(instance)}); }
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {InstId(instance), flag}); }
void StopFlashingObject(tag_sRideAnimSet* set) { ev("StopFlashingObject", {SetId(set)}); }
void StartFlashingObject(tag_sRideAnimSet* set) { ev("StartFlashingObject", {SetId(set)}); }
float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    ev("DistanceBetweenPoints", {PointBits(a, 0), PointBits(a, 1), PointBits(a, 2), PointBits(b, 0), PointBits(b, 1), PointBits(b, 2)});
    return BitsFloat(ret("DistanceBetweenPoints"));
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    double d1 = va_arg(ap, double);
    double d2 = va_arg(ap, double);
    va_end(ap);
    uint64_t b1, b2;
    std::memcpy(&b1, &d1, 8);
    std::memcpy(&b2, &d2, 8);
    int64_t matched = std::strcmp(format, "Curviness %.2f - Straight Divisor %.2f\n") == 0 ? 1 : 0;
    ev("CTRACE", {level, matched, static_cast<int64_t>(b1 >> 32), static_cast<int64_t>(b1 & 0xffffffffu), static_cast<int64_t>(b2 >> 32),
                  static_cast<int64_t>(b2 & 0xffffffffu)});
}

extern "C" {

uint64_t tpw_gts2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterEditInstance", &gCoasterEditInstance},
        {"gExtrudeTOffset", &gExtrudeTOffset},
        {"gRideAnimSets", gRideAnimSets},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gts2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gts2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF30));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mSpeedLimit));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mLastBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBrace64));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlashBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTypeID));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF08));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF10));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF4C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF50));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mX4C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF94w));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF90));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(sExtrusionParams));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mL50));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mZ54));
    return n;
}

int32_t tpw_gts2_gen(uint64_t a, uint64_t b, uint64_t params, uint64_t out, float span) {
    return GenerateTrackSubdivisions(reinterpret_cast<tag_sTrackBrace*>(a), reinterpret_cast<tag_sTrackBrace*>(b),
                                     reinterpret_cast<sExtrusionParams*>(params), reinterpret_cast<float*>(out), span);
}
void tpw_gts2_speed(uint64_t inst, uint32_t speed, int32_t flag) { CoasterSetSpeed(reinterpret_cast<tag_sCoasterInstance*>(inst), speed, flag); }
float tpw_gts2_bank(uint64_t inst, uint64_t brace, float f) {
    return GetBankAtBrace(reinterpret_cast<tag_sCoasterInstance*>(inst), reinterpret_cast<tag_sTrackBrace*>(brace), f);
}
void tpw_gts2_flash(uint64_t inst, uint32_t mode) { FlashEndPylons(reinterpret_cast<tag_sCoasterInstance*>(inst), mode); }
int32_t tpw_gts2_dec() { return DecCoasterConst1(); }

}  // extern "C"
