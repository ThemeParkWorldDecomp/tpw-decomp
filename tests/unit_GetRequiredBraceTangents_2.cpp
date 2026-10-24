// Test library for GetRequiredBraceTangents (unit v51024; libtpw_unit_GetRequiredBraceTangents_2.dylib, tests/test_GetRequiredBraceTangents_2.py), main app:
//   src/game/rides/CoasterPylon.cpp   GetRequiredBraceTangents and the two clamp constants
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: NormaliseVectorXZ and MathLib::__fpclassifyf.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

namespace MathLib {
int __fpclassifyf(float x) {
    ev("fpclassifyf", {CanonBits(x)});
    return x != x ? 1 : 4;
}
}  // namespace MathLib

void NormaliseVectorXZ(s3DPoint& p) {
    ev("NormaliseVectorXZ", {CanonBits(p.x), CanonBits(p.y), CanonBits(p.z)});
    const double x = p.x, z = p.z;
    const double len = std::sqrt(x * x + z * z);
    if (len != 0) {
        p.x = static_cast<float>(x / len);
        p.z = static_cast<float>(z / len);
    }
}

extern "C" {

uint64_t tpw_gbt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gbt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mX4C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mL50));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mZ54));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF94w));
    out[n++] = static_cast<int64_t>(sizeof(sTrackBraceData));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF18Table));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterTrackParams, mBraceHead));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterTrackParams));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterType));
    out[n++] = 0;   // the type's words by their Mac offsets
    out[n++] = 4;
    out[n++] = 8;
    out[n++] = 0x0c;
    return n;
}

void tpw_gbt_call(uint64_t brace, uint64_t type, uint64_t params) {
    GetRequiredBraceTangents(reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)),
                             reinterpret_cast<sCoasterType*>(static_cast<uintptr_t>(type)),
                             reinterpret_cast<sCoasterTrackParams*>(static_cast<uintptr_t>(params)));
}

}  // extern "C"
