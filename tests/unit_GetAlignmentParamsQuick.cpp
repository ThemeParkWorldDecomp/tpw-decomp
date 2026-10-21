// Test library for the coaster Bezier and alignment helpers (unit v5970; libtpw_unit_GetAlignmentParamsQuick.dylib,
// tests/test_GetAlignmentParamsQuick.py), main app:
//   src/game/rides/CoasterSpline.cpp   BezierPosition, BezierTangent, GetAlignmentParamsQuick, GetBankParams, GetAlignmentParamsSlow
//
// The callees outside the unit (the three rotation getters, SetInitialOrientationParams) are fakes that record their arguments and
// answer from the test's queues; gTPGlobals is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPGlobals.h"
#include "rsse.h"

using namespace calllog;

// gTPGlobals as raw zeroed storage under its symbol: its constructor (and its string members') are in files this library does not compile.
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] asm("_gTPGlobals");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t v) {
    uint32_t b = static_cast<uint32_t>(v);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

template <class T>
T* P(uint64_t a) { return reinterpret_cast<T*>(static_cast<uintptr_t>(a)); }

float Rot(const char* name, s3DPoint& p) {
    ev(name, {CanonBits(p.x), CanonBits(p.y), CanonBits(p.z)});
    return FromBits(ret(name));
}

}  // namespace

float GetVectorRotationAroundXAxisFromZ(s3DPoint& point) { return Rot("RotX", point); }
float GetVectorRotationAroundYAxisFromZ(s3DPoint& point) { return Rot("RotY", point); }
float GetVectorRotationAroundZAxisFromY(s3DPoint& point) { return Rot("RotZ", point); }

void SetInitialOrientationParams(sCoasterOrientation* orient, sCoasterTrackParams* params, int32_t a) {
    ev("SetInit", {lab(orient), lab(params), a});
}

extern "C" {

uint64_t tpw_gap_addr(const char* name) {
    if (std::strcmp(name, "tpg") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTPGlobalsStorage));
    return 0;
}

uint64_t tpw_gap_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gap_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(sCoasterOrientation, mF04));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterOrientation, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterOrientation, mF10));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterOrientation));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableMask));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTable));
    out[n++] = static_cast<int64_t>(offsetof(tag_sBezierControl, mF40));
    out[n++] = static_cast<int64_t>(sizeof(tag_sBezierControl));
    return n;
}


void tpw_gap_bpos(uint64_t a, uint64_t b, float t, uint64_t out) { BezierPosition(P<tag_sBezierControl>(a), P<tag_sBezierControl>(b), t, P<s3DPoint>(out)); }
void tpw_gap_btan(uint64_t a, uint64_t b, float t, uint64_t out) { BezierTangent(P<tag_sBezierControl>(a), P<tag_sBezierControl>(b), t, P<s3DPoint>(out)); }
void tpw_gap_quick(float x, float y, float z, float twist, uint64_t orient, uint64_t out) {
    s3DPoint p{x, y, z};
    GetAlignmentParamsQuick(p, twist, P<sCoasterOrientation>(orient), P<float>(out));
}
void tpw_gap_bank(uint64_t pt, uint64_t a, uint64_t b, float t, uint64_t orient) {
    GetBankParams(P<s3DPoint>(pt), P<tag_sBezierControl>(a), P<tag_sBezierControl>(b), t, P<sCoasterOrientation>(orient));
}
void tpw_gap_slow(uint64_t params, uint64_t pt, float f, uint64_t orient, int32_t n, uint64_t out) {
    GetAlignmentParamsSlow(P<sCoasterTrackParams>(params), P<s3DPoint>(pt), f, P<sCoasterOrientation>(orient), n, P<float>(out));
}

}  // extern "C"
