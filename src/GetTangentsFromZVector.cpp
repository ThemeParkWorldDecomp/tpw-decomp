// 0x1003be20 (1,784 bytes, TPCoaster.cpp): the two tangent vectors of a Bezier control from the Z vector of the track point.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"

// The imports reached through the MSL glue (faked in the tests).
namespace MathLib {
double sqrt(double x);                  // glue 0x10239250
int __fpclassifyf(float x);             // glue 0x10239268
double fmod(double x, double y);        // glue 0x102399e8
}  // namespace MathLib

namespace {

// The control's floats sit at Mac offsets 0x1c..0x54; the host struct has a pointer over some of them, so they are read and
// written by byte offset. The same goes for the brace's floats (0x4c, 0x54, 0xec, 0xf8).
float RdF(const void* base, size_t offset) {
    float v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, sizeof v);
    return v;
}

void WrF(void* base, size_t offset, float v) { std::memcpy(static_cast<uint8_t*>(base) + offset, &v, sizeof v); }

float BraceF4C(const tag_sTrackBrace* b) { return RdF(b->mF0C, 0x4c - 0x0c); }
float BraceF54(const tag_sTrackBrace* b) { return RdF(b->mF0C, 0x54 - 0x0c); }
float BraceFEC(const tag_sTrackBrace* b) { return RdF(b->mFE8, 0xec - 0xe8); }
float BraceFF8(const tag_sTrackBrace* b) { return RdF(b->mFE8, 0xf8 - 0xe8); }

// The length of the vector at c+off: fmul (double) of the first, fmuls of the others, fadd, sqrt, frsp.
float Length(void* c, size_t off) {
    float x = RdF(c, off), y = RdF(c, off + 4), z = RdF(c, off + 8);
    double sx = static_cast<double>(x) * static_cast<double>(x);
    float sy = y * y;
    float sz = z * z;
    double s = sx + static_cast<double>(sy);
    s = static_cast<double>(sz) + s;
    return static_cast<float>(MathLib::sqrt(s));
}

void Scale(void* c, size_t off, float s) {
    WrF(c, off, RdF(c, off) * s);
    WrF(c, off + 4, RdF(c, off + 4) * s);
    WrF(c, off + 8, RdF(c, off + 8) * s);
}

// The std::max(x, 0.0f) and std::min(x, 1.0f) of the original (references to the constants). The branch is bge after fcmpo, which is taken
// unless "less than": a NaN keeps x.
float ClampLow(float x) { return (x < 0.0f) ? 0.0f : x; }
float ClampHigh(float x) { return (1.0f < x) ? 1.0f : x; }

float Sin(uint32_t index) { return gTPGlobals.mSinTable[(index << 2) / 4]; }

}  // namespace

void GetTangentsFromZVector(tag_sCoasterType* coaster, s3DPoint* point, tag_sBezierControl* control, float lengthA, float lengthB,
                            sPylonControl* pylon, s3DPoint* unusedA, s3DPoint* unusedB, int32_t flag, int32_t unusedC) {
    (void)unusedA;
    (void)unusedB;
    (void)unusedC;
    (void)flag;  // both of its branches do the same
    const float f26 = lengthA;
    const float f27 = lengthB;
    WrF(control, 0x28, point->x);
    WrF(control, 0x2c, point->y);
    WrF(control, 0x30, point->z);
    WrF(control, 0x1c, -RdF(control, 0x28));
    WrF(control, 0x20, -RdF(control, 0x2c));
    WrF(control, 0x24, -RdF(control, 0x30));

    if (pylon != nullptr && (pylon->mFlags & 0x4000) != 0) {
        tag_sTrackBrace* brace = control->mBrace;
        float f28 = coaster->mF08;
        float f31, f30, f29;
        tag_sTrackBrace* other = (brace->mFlags & 0x80) ? brace->mPrev : brace->mNext;
        if (other != nullptr) {
            f31 = BraceF4C(other) - BraceF4C(brace);
            f30 = 0.0f;
            f29 = BraceF54(other) - BraceF54(brace);
        } else {
            f31 = 0.0f;
            f29 = 1.0f;
            f30 = f31;
        }
        float sq0 = f29 * f29;
        float sq1 = f30 * f30;
        double inner = ppc_fmadd(static_cast<double>(f31), static_cast<double>(f31), static_cast<double>(sq1));
        double total = static_cast<double>(sq0) + inner;
        float len = static_cast<float>(MathLib::sqrt(total));
        f31 = f31 / len;
        f30 = f30 / len;
        f29 = f29 / len;
        if (MathLib::__fpclassifyf(f31) == 1) {
            f31 = 0.0f;
            f29 = 1.0f;
            f30 = f31;
        }
        brace = control->mBrace;
        const uint32_t mask = gTPGlobals.mSinTableMask;
        const float scale = gTPGlobals.mSinTableScale;
        const float zero = 0.0f;
        float ang = 6.2831855f * BraceFEC(brace);
        float t1 = ang * scale;
        float t2 = (1.5707964f + ang) * scale;
        int32_t i1 = ppc_fctiwz(static_cast<double>(t1));
        int32_t i2 = ppc_fctiwz(static_cast<double>(t2));
        float v0 = f30 * zero;
        float s1 = Sin(mask & static_cast<uint32_t>(i1));
        float s2 = Sin(mask & static_cast<uint32_t>(i2));
        float m1 = -1.0f * s1;
        float f5 = ppc_fmadds(zero, s2, m1);
        float m2 = -1.0f * s2;
        v0 = ppc_fmadds(f31, f5, v0);
        float f1v = ppc_fmadds(-zero, s1, m2);
        v0 = ppc_fmadds(f29, f1v, v0);
        float t = std::fabs(v0);
        t = ClampLow(t);
        t = ClampHigh(t);
        float sq = t * t;
        float rest = 1.0f - sq;
        float part = 2.1f * sq;
        brace = control->mBrace;
        float bf8 = BraceFF8(brace);
        f28 = ppc_fmadds(f28, rest, part);
        float fm = static_cast<float>(MathLib::fmod(static_cast<double>(bf8), 0.5));
        float u = 2.0f * fm;
        u = ClampLow(u);
        u = ClampHigh(u);
        float a3 = ppc_fmadds(6.2831855f, u, 1.5707964f);
        int32_t i3 = ppc_fctiwz(static_cast<double>(a3 * scale));
        float s3 = Sin(gTPGlobals.mSinTableMask & static_cast<uint32_t>(i3));
        float e = (1.0f + s3) * 0.5f;
        brace = control->mBrace;
        float rest2 = 1.0f - e;
        float part2 = 2.1f * e;
        const bool flagSet = (brace->mFlags & 0x80) != 0;
        f28 = ppc_fmadds(f28, rest2, part2);
        if (flagSet) {
            float l = Length(control, 0x1c);
            tag_sTrackBrace* nxt = control->mBrace->mNext;
            float s;
            if (nxt != nullptr && (nxt->mFlags & 0x40) != 0)
                s = (f26 * f28) / l;
            else
                s = (1.7f * f26) / l;
            Scale(control, 0x1c, s);
            l = Length(control, 0x28);
            s = (f27 * f28) / l;
            Scale(control, 0x28, s);
        } else {
            float l = Length(control, 0x1c);
            float s = (f26 * f28) / l;
            Scale(control, 0x1c, s);
            l = Length(control, 0x28);
            tag_sTrackBrace* prv = control->mBrace->mPrev;
            if (prv != nullptr && (prv->mFlags & 0x40) != 0)
                s = (f27 * f28) / l;
            else
                s = (1.7f * f27) / l;
            Scale(control, 0x28, s);
        }
    } else {
        float l = Length(control, 0x1c);
        float s = (f26 * coaster->mF08) / l;
        Scale(control, 0x1c, s);
        l = Length(control, 0x28);
        s = (f27 * coaster->mF08) / l;
        Scale(control, 0x28, s);
    }

    for (int i = 0; i < 3; ++i) {
        WrF(control, 0x34 + 4 * i, RdF(control, 0x40 + 4 * i) + RdF(control, 0x1c + 4 * i) / 3.0f);
        WrF(control, 0x4c + 4 * i, RdF(control, 0x40 + 4 * i) + RdF(control, 0x28 + 4 * i) / 3.0f);
    }
}
