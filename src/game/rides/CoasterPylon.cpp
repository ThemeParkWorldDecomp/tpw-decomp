// CoasterPylon.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "BMP_InsideArea.h"
#include "CoasterBalance.h"
#include "Debug.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace MathLib {
int __fpclassifyf(float x);  // glue 0x10239268
}
void NormaliseVectorXZ(s3DPoint& point);   // 0x10045a38

// The two constants of the clamp (the TOC slots -0x5528 and -0x5524 point at them; names invented).
float gBraceTangentOne = 1.0f;         // 0x1029cd1c
float gBraceTangentMinusOne = -1.0f;   // 0x1029cd18

namespace {

// The brace's words in the Mac's +0x0c..+0x63 range keep their Mac offsets in mF0C on the host. The position (Mac +0x4c..+0x54)
// is the appended mX4C, mL50, mZ54 (the fields the other units fill).
float Ld(const tag_sTrackBrace* brace, uint32_t macOffset) {
    float v;
    std::memcpy(&v, brace->mF0C + (macOffset - 0x0c), 4);
    return v;
}
void St(tag_sTrackBrace* brace, uint32_t macOffset, float v) {
    std::memcpy(brace->mF0C + (macOffset - 0x0c), &v, 4);
}
float LdW(const void* base, uint32_t offset) {
    float v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}
uint32_t LdU(const void* base, uint32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}

// The brace's pylon record word: its +0 holds the flags, its +0xb0 a float limit.
const uint32_t* Table(const tag_sTrackBrace* brace) {
    return reinterpret_cast<const uint32_t*>(brace->mData->mF18Table);
}

// fmuls, fmuls, fmadd, fadd, sqrt, frsp.
float Length(float x, float y, float z) {
    const float yy = y * y;
    const float zz = z * z;
    const double inner = ppc_fmadd(static_cast<double>(x), static_cast<double>(x), static_cast<double>(yy));
    return static_cast<float>(std::sqrt(static_cast<double>(zz) + inner));
}

// True when the brace's own pylon record has no flag 0x4000 and a neighbour's has it: the y components are flattened.
bool FlattenNeeded(const tag_sTrackBrace* brace) {
    const uint32_t* own = Table(brace);
    if (own == nullptr || (own[0] & 0x4000) != 0) return false;
    const tag_sTrackBrace* next = brace->mNext;
    if (next != nullptr) {
        const uint32_t* t = Table(next);
        if (t != nullptr && (t[0] & 0x4000) != 0) return true;
    }
    const tag_sTrackBrace* prev = brace->mPrev;
    if (prev == nullptr) return false;
    const uint32_t* t = Table(prev);
    return t != nullptr && (t[0] & 0x4000) != 0;
}

}  // namespace

// 0x1003aef4 (3048 bytes, TPCoaster.cpp). Works out the two tangents of a brace (Mac +0x28 and +0x34) from its neighbours (the next one at
// +0x68, the previous at +0x64, either gone by flags 0x80000 / 0x40000; flag 0x100000 takes the previous of the params' first brace),
// the angle between the two legs (+0x94), and the two control points (+0x40, +0x58) a third of a tangent from the position.
// The params' word the original reads at Mac +0x18 is mBraceHead here (host offset 0x18).
void GetRequiredBraceTangents(tag_sTrackBrace* brace, sCoasterType* type, sCoasterTrackParams* params) {
    float* one = &gBraceTangentOne;
    float* minusOne = &gBraceTangentMinusOne;
    const uint32_t* data = Table(brace);
    const uint32_t flags0 = brace->mFlags;
    tag_sTrackBrace* nextB = (flags0 & 0x80000) == 0 ? brace->mNext : nullptr;
    tag_sTrackBrace* prevB = (flags0 & 0x40000) == 0 ? brace->mPrev : nullptr;
    if ((flags0 & 0x100000) != 0) prevB = params->mBraceHead->mPrev;

    float t1[3] = {Ld(brace, 0x28), Ld(brace, 0x2c), Ld(brace, 0x30)};   // stack 0x90
    float t2[3] = {Ld(brace, 0x34), Ld(brace, 0x38), Ld(brace, 0x3c)};   // stack 0x84
    brace->mFlags = brace->mFlags & 0xf1ffffffu;
    if (nextB == nullptr && prevB == nullptr) return;

    const float px = brace->mX4C, py = brace->mL50, pz = brace->mZ54;
    const float typeScale = LdW(type, 0x0c);
    float ax = 0.0f, ay = 0.0f, az = 0.0f;   // stack 0xc0
    float bx = 0.0f, by = 0.0f, bz = 0.0f;   // stack 0xb4
    if (nextB != nullptr) {
        ax = nextB->mX4C - px;
        ay = nextB->mL50 - py;
        az = nextB->mZ54 - pz;
    }
    if (prevB != nullptr) {
        bx = prevB->mX4C - px;
        by = prevB->mL50 - py;
        bz = prevB->mZ54 - pz;
    }

    if (prevB == nullptr) {
        if (data != nullptr && (data[0] & 0x4000) != 0 && (brace->mFlags & 0x80) == 0) {
            tag_sTrackBrace* nn = nextB->mNext;
            if (nn != nullptr) {
                float f5 = nextB->mL50 - nn->mL50;
                float f4 = nextB->mL50 - py;
                float f1 = f5 * f4;
                float f7 = nextB->mX4C - nn->mX4C;
                float f6 = nextB->mX4C - px;
                float g4 = nextB->mZ54 - nn->mZ54;
                float g2 = nextB->mZ54 - pz;
                f1 = ppc_fmadds(f7, f6, f1);
                f1 = ppc_fmadds(g4, g2, f1);
                if (f1 < LdW(data, 0xb0)) {
                    t2[0] = ax / typeScale;
                    t2[1] = ay / typeScale;
                    t2[2] = az / typeScale;
                    t1[0] = -t2[0];
                    t1[1] = -t2[1];
                    t1[2] = -t2[2];
                    // goto the angle
                    brace->mF94w = 0.0f;
                    goto store;
                }
            }
        }
        t1[0] = ax / typeScale;
        t1[1] = ay / typeScale;
        t1[2] = az / typeScale;
        t2[0] = -t1[0];
        t2[1] = -t1[1];
        t2[2] = -t1[2];
        // The generic case only flattens when the first test (data with flag 0x4000 and brace flag 0x80 clear) failed; the dot-product
        // case with no second brace (nn == NULL) or a big dot takes the same path without it.
        if (!(data != nullptr && (data[0] & 0x4000) != 0 && (brace->mFlags & 0x80) == 0)) {
            if (FlattenNeeded(brace)) {
                t1[1] = 0.0f;
                t2[1] = 0.0f;
            }
        }
        brace->mF94w = 0.0f;
    } else if (nextB == nullptr) {
        t2[0] = bx / typeScale;
        t2[1] = by / typeScale;
        t2[2] = bz / typeScale;
        t1[0] = -t2[0];
        t1[1] = -t2[1];
        t1[2] = -t2[2];
        brace->mF94w = 0.0f;
    } else {
        float n1[3] = {ax, ay, az};   // stack 0xa8
        float len = Length(n1[0], n1[1], n1[2]);
        n1[0] = n1[0] / len;
        n1[1] = n1[1] / len;
        n1[2] = n1[2] / len;
        float n2[3] = {bx, by, bz};   // stack 0x9c
        len = Length(n2[0], n2[1], n2[2]);
        n2[0] = n2[0] / len;
        n2[1] = n2[1] / len;
        n2[2] = n2[2] / len;
        const float lenA = Length(ax, ay, az);   // f31
        const float lenB = Length(bx, by, bz);   // f30
        float dot = n1[1] * n2[1];
        dot = ppc_fmadds(n1[0], n2[0], dot);
        dot = ppc_fmadds(n1[2], n2[2], dot);

        s3DPoint p1 = {n1[0], n1[1], n1[2]};
        s3DPoint p2 = {n2[0], n2[1], n2[2]};
        NormaliseVectorXZ(p1);
        NormaliseVectorXZ(p2);
        float c = p1.z * p2.z;
        c = ppc_fmadds(p1.x, p2.x, c);
        if (*minusOne < c) minusOne = &c;
        c = *minusOne;
        if (c < *one) one = &c;
        c = *one;
        const float cross = ppc_fmsubs(p1.z, p2.x, p1.x * p2.z);
        if (MathLib::__fpclassifyf(cross) == 1 || !(cross < 0.0f)) {
            brace->mF94w = static_cast<float>(std::acos(static_cast<double>(c)));
        } else {
            brace->mF94w = -static_cast<float>(std::acos(static_cast<double>(c)));
        }

        bool sharp;
        if (data != nullptr && dot < LdW(data, 0xb0)) sharp = true;
        else if (data == nullptr && dot < -0.999f) sharp = true;
        else sharp = 0.999f < dot;

        if (sharp) {
            if (data != nullptr && (data[0] & 0x4000) != 0) {
                const float half = 0.5f;
                const float dx = (n1[0] - n2[0]) * half / typeScale;
                const float dy = (n1[1] - n2[1]) * half / typeScale;
                const float dz = (n1[2] - n2[2]) * half / typeScale;
                const float ex = (n2[0] - n1[0]) * half / typeScale;
                const float ey = (n2[1] - n1[1]) * half / typeScale;
                const float ez = (n2[2] - n1[2]) * half / typeScale;
                if ((brace->mFlags & 0x80) != 0) {
                    t1[0] = lenA * dx;
                    t2[0] = lenB * ex;
                    t1[1] = lenA * dy;
                    t2[1] = lenB * ey;
                    t1[2] = lenA * dz;
                    t2[2] = lenB * ez;
                } else {
                    t2[0] = lenB * dx;
                    t1[0] = lenA * ex;
                    t2[1] = lenB * dy;
                    t1[1] = lenA * ey;
                    t2[2] = lenB * dz;
                    t1[2] = lenA * ez;
                }
            } else {
                t1[0] = ax / typeScale;
                t1[1] = ay / typeScale;
                t1[2] = az / typeScale;
                t2[0] = bx / typeScale;
                t2[1] = by / typeScale;
                t2[2] = bz / typeScale;
                if (FlattenNeeded(brace)) {
                    t1[1] = 0.0f;
                    t2[1] = 0.0f;
                }
            }
        } else {
            float s[3] = {n1[0] + n2[0], n1[1] + n2[1], n1[2] + n2[2]};   // stack 0x78
            if (data != nullptr && (data[0] & 0x4000) != 0) {
                t1[0] = s[0];
                t1[1] = s[1];
                t1[2] = s[2];
                t2[0] = -t1[0];
                t2[1] = -t1[1];
                t2[2] = -t1[2];
                const float scale = LdW(type, 8);
                const float k1 = lenA * scale;
                const float k2 = lenB * scale;
                t1[0] = t1[0] * k1;
                t1[1] = t1[1] * k1;
                t1[2] = t1[2] * k1;
                t2[0] = t2[0] * k2;
                t2[1] = t2[1] * k2;
                t2[2] = t2[2] * k2;
            } else {
                float cx = ppc_fmsubs(ay, bz, az * by);   // f29
                float cy = ppc_fmsubs(az, bx, ax * bz);   // f28
                float cz = ppc_fmsubs(ax, by, ay * bx);   // f27
                if ((LdU(type, 0) & 8) != 0 && (brace->mFlags & 0x40) == 0 && cy < 0.0f) {
                    brace->mFlags = brace->mFlags | 0x10000000u;
                } else {
                    brace->mFlags = brace->mFlags & 0xefffffffu;
                }
                const float clen = Length(cx, cy, cz);
                cx = cx / clen;
                cy = cy / clen;
                cz = cz / clen;
                const float slen = Length(s[0], s[1], s[2]);
                s[0] = s[0] / slen;
                s[1] = s[1] / slen;
                s[2] = s[2] / slen;
                const double sx = s[0], sy = s[1], sz = s[2];
                const double dcx = cx, dcy = cy, dcz = cz;
                const double r0 = sy * dcz - sz * dcy;
                const double r1 = sz * dcx - sx * dcz;
                const double r2 = sx * dcy - sy * dcx;
                t1[0] = static_cast<float>(r0);
                t1[1] = static_cast<float>(r1);
                t1[2] = static_cast<float>(r2);
                t2[0] = -t1[0];
                t2[1] = -t1[1];
                t2[2] = -t1[2];
                const float scale = LdW(type, 8);
                const float k1 = lenA * scale;
                const float k2 = lenB * scale;
                t1[0] = t1[0] * k1;
                t1[1] = t1[1] * k1;
                t1[2] = t1[2] * k1;
                t2[0] = t2[0] * k2;
                t2[1] = t2[1] * k2;
                t2[2] = t2[2] * k2;
                if (FlattenNeeded(brace)) {
                    t1[1] = 0.0f;
                    t2[1] = 0.0f;
                }
            }
        }
    }

store:
    St(brace, 0x28, t1[0]);
    St(brace, 0x2c, t1[1]);
    St(brace, 0x30, t1[2]);
    St(brace, 0x34, t2[0]);
    St(brace, 0x38, t2[1]);
    St(brace, 0x3c, t2[2]);
    const float three = 3.0f;
    St(brace, 0x40, px + Ld(brace, 0x28) / three);
    St(brace, 0x44, py + Ld(brace, 0x2c) / three);
    St(brace, 0x48, pz + Ld(brace, 0x30) / three);
    St(brace, 0x58, px + Ld(brace, 0x34) / three);
    St(brace, 0x5c, py + Ld(brace, 0x38) / three);
    St(brace, 0x60, pz + Ld(brace, 0x3c) / three);
}

// 0x1003bb44 (652 bytes, TPCoaster.cpp): SetupBraceAnimFrameData. Sets the five time controls (records of 0x38 bytes) of the
// brace's ride anim set: each value (the loft, the second value, the wobble, and two more) is scaled by a table of the
// pylon record's +0x18 block, clamped to a range and multiplied by the record's float at +0x1c, stored at +0x20.

namespace {

float Clamp(float v, float lo, float hi) {
    if (v < lo) v = lo;   // fcmpo / bge: a NaN branches over
    if (v > hi) v = hi;
    return v;
}

void Control(tag_sRideAnimSet* set, uint32_t offset, float value, const float* t, float add) {
    float v = value * t[2];
    v = v + t[3];
    v = v + add;
    v = Clamp(v, t[0], t[1]);
    float* rec = reinterpret_cast<float*>(static_cast<char*>(set->mTimeControls) + offset);
    rec[0x20 / 4] = v * rec[0x1c / 4];
}

}  // namespace

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) {
    (void)type;
    uint32_t index = brace->mWord70;
    const sTrackBraceData* data = brace->mData;
    if ((data->mFlags & 1) != 0) return;
    if ((brace->mFlags & 0x2000) != 0) return;
    const float* table = reinterpret_cast<const float*>(reinterpret_cast<const char*>(data->mF18Table) + 0xd0);
    float loft = brace->mFE8w;
    if (loft > 1.0f) brace->mFE8w = 1.0f;
    else if (loft < 0.0f) brace->mFE8w = 0.0f;
    double whole;
    float frac = static_cast<float>(std::modf(static_cast<double>(brace->mFF8), &whole));
    if (frac < 0.0f) frac = 1.0f + frac;
    brace->mFF8 = frac;
    tag_sRideAnimSet* set = gRideAnimSets[index];
    Control(set, 0x00, brace->mFE8w, table + 0, 0.0f);
    float second = brace->mFEC;
    if ((brace->mFlags & 0x10000000) != 0) {
        float sum = second + 0.5f;
        second = static_cast<float>(std::modf(static_cast<double>(sum), &whole));
    }
    Control(set, 0x38, second, table + 4, 0.0f);
    Control(set, 0xe0, brace->mFF8, table + 16, 0.0f);
    Control(set, 0x70, brace->mFF0, table + 8, 0.5f);
    Control(set, 0xa8, brace->mFF4, table + 12, 0.5f);
}

// 0x1003be20 (1,784 bytes, TPCoaster.cpp): the two tangent vectors of a Bezier control from the Z vector of the track point.

// The imports reached through the MSL glue (faked in the tests).
namespace MathLib {
double sqrt(double x);                  // glue 0x10239250
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
float Length_2(void* c, size_t off) {
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
            float l = Length_2(control, 0x1c);
            tag_sTrackBrace* nxt = control->mBrace->mNext;
            float s;
            if (nxt != nullptr && (nxt->mFlags & 0x40) != 0)
                s = (f26 * f28) / l;
            else
                s = (1.7f * f26) / l;
            Scale(control, 0x1c, s);
            l = Length_2(control, 0x28);
            s = (f27 * f28) / l;
            Scale(control, 0x28, s);
        } else {
            float l = Length_2(control, 0x1c);
            float s = (f26 * f28) / l;
            Scale(control, 0x1c, s);
            l = Length_2(control, 0x28);
            tag_sTrackBrace* prv = control->mBrace->mPrev;
            if (prv != nullptr && (prv->mFlags & 0x40) != 0)
                s = (f27 * f28) / l;
            else
                s = (1.7f * f27) / l;
            Scale(control, 0x28, s);
        }
    } else {
        float l = Length_2(control, 0x1c);
        float s = (f26 * coaster->mF08) / l;
        Scale(control, 0x1c, s);
        l = Length_2(control, 0x28);
        s = (f27 * coaster->mF08) / l;
        Scale(control, 0x28, s);
    }

    for (int i = 0; i < 3; ++i) {
        WrF(control, 0x34 + 4 * i, RdF(control, 0x40 + 4 * i) + RdF(control, 0x1c + 4 * i) / 3.0f);
        WrF(control, 0x4c + 4 * i, RdF(control, 0x40 + 4 * i) + RdF(control, 0x28 + 4 * i) / 3.0f);
    }
}

// The track editor's brace placement: where the two control arrays of a brace stand (GetActualBraceTrackPositions), the tangents of
// the controls (GetActualBraceTangents) and the lengths of the spans next to a control (GetTangentLengths). TPCoaster.cpp on the Mac.

float gBraceTrackScale2 = 2.0f;   // 0x1029cd14

namespace {

// The node a ride set's mesh table holds for `index` (set +8 -> +0x28 -> +4 -> index * 0x14 + 4).
sMatrix* RideNode(tag_sRideAnimSet* set, uint32_t index) {
    tagBumpHeadTable* table = static_cast<tagBumpHeadTable*>(set->mHeads->mF28);
    return reinterpret_cast<sMatrix*>(table->entries[index].node);
}

uint32_t ControlWord4(const tag_sBezierControl* control) {
    uint32_t word;
    std::memcpy(&word, control->mF00 + 4, 4);
    return word;
}

float TableAt(float angleTimesScale) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(angleTimesScale))];
}

// The length of the span from q to p with y squeezed by 0.7: fmsubs, fmuls, fmadd, fadd, sqrt, frsp.
float SpanLength(const s3DPoint& p, const s3DPoint& q) {
    const float k = 0.699999988079071f;
    float f0 = q.y * k;
    float f3 = ppc_fmsubs(p.y, k, f0);
    float dx = p.x - q.x;
    float dz = p.z - q.z;
    f3 = f3 * f3;
    dz = dz * dz;
    double sum = ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(f3));
    sum = static_cast<double>(dz) + sum;
    return static_cast<float>(std::sqrt(sum));
}

void Difference(s3DPoint* out, const s3DPoint& q, const s3DPoint& p) {
    if (out == nullptr) return;
    out->x = q.x - p.x;
    out->y = q.y - p.y;
    out->z = q.z - p.z;
}

void Normalise(float* v) {
    double sum = static_cast<double>(v[0]) * static_cast<double>(v[0]);
    float yy = v[1] * v[1];
    float zz = v[2] * v[2];
    sum = sum + static_cast<double>(yy);
    sum = static_cast<double>(zz) + sum;
    float length = static_cast<float>(std::sqrt(sum));
    v[0] = v[0] / length;
    v[1] = v[1] / length;
    v[2] = v[2] / length;
}

}  // namespace

// 0x1003c59c: the lengths of the spans before (*before) and after (*after) control `index` of the brace's array (A when `useA`, else B),
// and the span vectors. The first span of the first control comes from the next brace's last control, the last control's second span
// from the previous brace's first one (the names of the two links are as the Mac's offsets: +0x68 and +0x64).
void GetTangentLengths(uint32_t index, tag_sTrackBrace* brace, float* before, float* after, int32_t useA, s3DPoint* beforeVec,
                       s3DPoint* afterVec) {
    tag_sTrackBrace* linkNext = brace->mNext;
    tag_sTrackBrace* linkPrev = brace->mPrev;
    tag_sBezierControl* nbr = nullptr;
    tag_sBezierControl* prevControls = nullptr;
    tag_sBezierControl* cur;
    if (useA != 0) {
        cur = brace->mF90->mControlsA;
        if (linkNext != nullptr) nbr = linkNext->mF90->mControlsA;
        if (linkPrev != nullptr) prevControls = linkPrev->mF90->mControlsA;
    } else {
        cur = brace->mF90->mControlsB;
        if (linkNext != nullptr) nbr = linkNext->mF90->mControlsB;
        if (linkPrev != nullptr) prevControls = linkPrev->mF90->mControlsB;
    }
    if (index == 0) {
        if (linkNext == nullptr) {
            *before = 1.0f;
        } else {
            const s3DPoint& p = nbr[linkNext->mF90->mCount - 1].mF40;
            const s3DPoint& q = cur[0].mF40;
            float length = SpanLength(p, q);
            Difference(beforeVec, q, p);
            *before = length;
        }
    } else {
        const s3DPoint& p = cur[index - 1].mF40;
        const s3DPoint& q = cur[index].mF40;
        float length = SpanLength(p, q);
        Difference(beforeVec, q, p);
        *before = length;
    }
    if (index == brace->mF90->mCount - 1) {
        if (linkPrev == nullptr) {
            *after = 1.0f;
        } else {
            const s3DPoint& p = cur[index].mF40;
            const s3DPoint& q = prevControls[0].mF40;
            float length = SpanLength(p, q);
            Difference(afterVec, q, p);
            *after = length;
        }
    } else {
        const s3DPoint& p = cur[index].mF40;
        const s3DPoint& q = cur[index + 1].mF40;
        float length = SpanLength(p, q);
        Difference(afterVec, q, p);
        *after = length;
    }
}

// 0x1003c950: for every control of the brace: the direction (from the ride set's node, the first control's, or straight up or down),
// the span lengths, then the tangents of both arrays' controls.
void GetActualBraceTangents(tag_sTrackBrace* brace, tag_sRideAnimSet* set) {
    tag_sCoasterType* type = const_cast<tag_sCoasterType*>(reinterpret_cast<const tag_sCoasterType*>(gCoasterCurrent->mType));
    sPylonControl* pylon = reinterpret_cast<sPylonControl*>(const_cast<float*>(brace->mData->mF18Table));
    tag_sBezierControl* a = brace->mF90->mControlsA;
    tag_sBezierControl* b = brace->mF90->mControlsB;
    for (uint32_t i = 0; i < brace->mF90->mCount; ++i) {
        float dir[3];
        if (set == nullptr) {
            if ((brace->mFlags & 0x40) == 0) {
                const uint8_t* first = reinterpret_cast<const uint8_t*>(brace->mF90->mControlsA);
                std::memcpy(&dir[0], first + 0x28, 4);
                std::memcpy(&dir[1], first + 0x2c, 4);
                std::memcpy(&dir[2], first + 0x30, 4);
                Normalise(dir);
            } else if ((brace->mFlags & 0x80) == 0) {
                dir[0] = 0.0f;
                dir[1] = -1.0f;
                dir[2] = 0.0f;
            } else {
                dir[0] = 0.0f;
                dir[1] = 1.0f;
                dir[2] = 0.0f;
            }
        } else {
            sMatrix* node = RideNode(set, ControlWord4(a));
            dir[0] = node->m[2][0];
            dir[1] = node->m[2][1];
            dir[2] = node->m[2][2];
            Normalise(dir);
        }
        float lengthA, lengthB, lengthC, lengthD;
        s3DPoint vecA, vecB;
        GetTangentLengths(i, brace, &lengthA, &lengthB, 1, &vecA, &vecB);
        GetTangentLengths(i, brace, &lengthC, &lengthD, 0, nullptr, nullptr);
        int32_t isFirst = (i == 0) ? 1 : 0;
        int32_t isLast = (brace->mF90->mCount - 1 == i) ? 1 : 0;
        GetTangentsFromZVector(type, reinterpret_cast<s3DPoint*>(dir), a, lengthA, lengthB, pylon, &vecA, &vecB, isFirst, isLast);
        GetTangentsFromZVector(type, reinterpret_cast<s3DPoint*>(dir), b, lengthC, lengthD, pylon, &vecA, &vecB, isFirst, isLast);
        ++a;
        ++b;
    }
}

// 0x1003cc34: the points of the brace's array B from the points of array A: straight above (or offset along the node's up axis), the
// bank from the node's axes. A brace on an end pylon (data flag 1) or with flag 0x2000 uses no node, only the yaw.
void GetActualBraceTrackPositions(tag_sRideAnimSet* set, tag_sTrackBrace* brace) {
    tag_sBezierControl* a = brace->mF90->mControlsA;
    tag_sBezierControl* b = brace->mF90->mControlsB;
    const float halfPi = 1.5707963705062866f;
    for (uint32_t i = 0; i < brace->mF90->mCount; ++i) {
        if ((brace->mData->mFlags & 1) != 0 || (brace->mFlags & 0x2000) != 0) {
            float x = 0.0f, y = 1.0f, z = 0.0f;
            if ((brace->mFlags & 0x40) != 0) {
                float scale = gTPGlobals.mSinTableScale;
                float angle = brace->mAngle;
                float f0 = halfPi + angle;
                float f2 = angle * scale;
                f0 = f0 * scale;
                float f4 = TableAt(f2);
                float f3 = TableAt(f0);
                x = ppc_fmadds(0.0f, f3, -1.0f * f4);
                y = 0.0f;
                z = ppc_fmadds(-0.0f, f4, -1.0f * f3);
            }
            b->mF40.x = a->mF40.x + x;
            b->mF40.y = a->mF40.y + y;
            b->mF40.z = a->mF40.z + z;
            continue;   // the original does not step the control pointers in this branch (only the counter)
        } else {
            sMatrix* node = RideNode(set, ControlWord4(a));
            if ((brace->mFlags & 0x10000000) != 0) {
                float scale = gTPGlobals.mSinTableScale;
                float f1 = static_cast<float>(-3.1415927410125732) * scale;
                float f0 = static_cast<float>(-1.5707963705062866) * scale;
                int32_t idxS = ppc_fctiwz(f1);
                int32_t idxC = ppc_fctiwz(f0);
                float c = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(idxC)];
                float s = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(idxS)];
                YRotateLMatrixNoTrans(node, c, s);
            }
            a->mF40.x = node->m[3][0];
            a->mF40.y = node->m[3][1];
            a->mF40.z = node->m[3][2];
            if ((brace->mFlags & 0x40) != 0) {
                b->mF40.x = ppc_fmadds(node->m[1][0], gBraceTrackScale2, a->mF40.x);
                b->mF40.y = ppc_fmadds(node->m[1][1], gBraceTrackScale2, a->mF40.y);
                b->mF40.z = ppc_fmadds(node->m[1][2], gBraceTrackScale2, a->mF40.z);
            } else {
                b->mF40.x = a->mF40.x;
                b->mF40.y = 1.0f + a->mF40.y;
                b->mF40.z = a->mF40.z;
                s3DPoint v1 = {node->m[1][0], node->m[1][1], node->m[1][2]};   // stack 0x44
                s3DPoint v2 = {node->m[2][0], node->m[2][1], node->m[2][2]};   // stack 0x38
                float scale = gTPGlobals.mSinTableScale;
                float angle = brace->mAngle;
                float nAngle = -angle;
                float s = TableAt(nAngle * scale);
                float c = TableAt((halfPi + nAngle) * scale);
                float v1x = v1.x;
                float t = v1.z * s;
                v1.x = ppc_fmadds(v1x, c, t);
                t = v1.z * c;
                v1.z = ppc_fmadds(-v1x, s, t);
                float v2x = v2.x;
                angle = brace->mAngle;
                nAngle = -angle;
                s = TableAt(nAngle * scale);
                c = TableAt((halfPi + nAngle) * scale);
                t = v2.z * s;
                v2.x = ppc_fmadds(v2x, c, t);
                t = v2.z * c;
                v2.z = ppc_fmadds(-v2x, s, t);
                float r = GetVectorRotationAroundXAxisFromZ(v2);
                float nr = -r;
                float rs = TableAt(nr * scale);
                float rc = TableAt((halfPi - r) * scale);
                float v1y = v1.y;
                t = v1.z * rs;
                v1.y = ppc_fmsubs(v1y, rc, t);
                t = v1.z * rc;
                v1.z = ppc_fmadds(v1y, rs, t);
                brace->mFE4 = GetVectorRotationAroundZAxisFromY(v1);
            }
        }
        ++a;
        ++b;
    }
}

// 0x1003d080 (1,056 bytes): recomputes the braces of the current coaster: their positions from their pylons, the required tangents, the
// alignment angles (yaw, pitch, bank) and the animated sets, then the actual tangents. The Mac reads the instance's +0x3c block
// (gCoasterCurrentData): its +0x10 is the instance's pylon list and +0x18 the chain of braces, here the instance's mPylons and mLastBrace.
// The brace's pylon (+0x100) is read as a sTrackBraceData: control at the mF18Table, x y z at 0x1c, the braces from the Mac's 0x34 (mOwner).
// Bug kept: the animated set of the previous brace is used by a brace that has none (r23 is not reset; NULL for the first).
void RecalcBraceTangents() {
    tag_sCoasterInstance* instance = gCoasterCurrent;
    sCoasterType* type = const_cast<sCoasterType*>(reinterpret_cast<const sCoasterType*>(instance->mType));
    sCoasterTrackParams* params = reinterpret_cast<sCoasterTrackParams*>(gCoasterCurrentData);
    tag_sRideAnimSet* set = nullptr;

    for (sCoasterPylon* pylon = instance->mPylons; pylon != nullptr; pylon = pylon->mNext) {
        for (uint32_t i = 0; i < pylon->mCount; ++i) {
            tag_sTrackBrace* brace = pylon->mBraces[i];
            const sTrackBraceData* data = brace->mData;
            sPylonControl* control = reinterpret_cast<sPylonControl*>(const_cast<float*>(data->mF18Table));
            if ((data->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0 && control->mF2C != -1) {
                std::memcpy(&brace->mX4C, &data->mF1C[0], 4);
                std::memcpy(&brace->mL50, &data->mF1C[4], 4);
                std::memcpy(&brace->mZ54, &data->mF1C[8], 4);
                const float five = 5.0f;
                brace->mX4C = brace->mX4C + five;
                brace->mZ54 = brace->mZ54 + five;
                uint32_t index;
                std::memcpy(&index, &brace->mF00[4], 4);
                if (index != 0) {
                    brace->mL50 = GetHeightForInterval(brace->mFE8w, control);
                    brace->mL50 = brace->mL50 + control->mFC4;
                    tag_sTrackBrace* const* braces = reinterpret_cast<tag_sTrackBrace* const*>(
                        reinterpret_cast<const uint8_t*>(brace->mData) + offsetof(sTrackBraceData, mOwner));
                    brace->mL50 = brace->mL50 + braces[index - 1]->mL50;
                } else {
                    float h = GetHeightForInterval(brace->mFE8w, control);
                    brace->mL50 = brace->mL50 + h;
                }
            }
        }
    }

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev)
        GetRequiredBraceTangents(brace, type, params);

    uint32_t typeFlags;
    std::memcpy(&typeFlags, type, 4);
    uint32_t banked = typeFlags & 0x10;
    float speed = 0.0f;
    float typeF48;
    std::memcpy(&typeF48, &type->mF00[0x48], 4);
    const float twoPi = 6.2831854820251465f;
    const float half = 0.5f;
    const float one = 1.0f;

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        tag_sTrackBrace* next = brace->mPrev;
        s3DPoint point;
        std::memcpy(&point, &brace->mF0C[0x34 - 0x0c], 12);
        sCoasterOrientation orientation;
        std::memset(&orientation, 0, sizeof(orientation));
        GetAlignmentParamsSlow(params, &point, brace->mF6C, &orientation, 1, nullptr);
        brace->mFD4 = orientation.mF04;
        if ((brace->mFlags & 1) == 0) brace->mAngle = orientation.mF04;
        if ((brace->mFlags & 2) == 0) brace->mFDC = orientation.mF08;
        if ((brace->mFlags & 4) == 0) brace->mFD8 = 0.0f;
        if (gCoasterWord7310 != 0) {
            uint32_t number = brace->mNumber;
            CTRACE(5, "Required brace %d params: Dirn %+.2f  Pitch %+.2f\n", number, static_cast<double>(orientation.mF04),
                   static_cast<double>(orientation.mF08));
        }
        if (banked != 0 && (brace->mFlags & 4) == 0 && next != nullptr) {
            brace->mFD8 = GetBankAtBrace(gCoasterCurrent, brace, speed);
            speed = GetCarWantedSpeedAtHeight(type, speed, typeF48, brace->mL50, next->mL50);
        }
        brace->mFEC = brace->mAngle / twoPi;
        double whole;
        float frac = static_cast<float>(std::modf(static_cast<double>(brace->mFEC), &whole));
        if (frac < 0.0f) frac = one + frac;
        brace->mFEC = frac;
        brace->mFF0 = brace->mFDC / twoPi;
        brace->mFF4 = brace->mFD8;
        brace->mFF4 = brace->mFF4 / twoPi;
        float shifted = half + brace->mFF4;
        frac = static_cast<float>(std::modf(static_cast<double>(shifted), &whole));
        if (frac < 0.0f) frac = one + frac;
        brace->mFF4 = frac - half;
        if ((brace->mData->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0) {
            SetupBraceAnimFrameData(type, brace);
            set = gRideAnimSets[brace->mWord70];
            AnimateRideSet(set, 1, 0);
        }
        GetActualBraceTrackPositions(set, brace);
    }

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        tag_sRideAnimSet* animSet = nullptr;
        if ((brace->mData->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0) animSet = gRideAnimSets[brace->mWord70];
        GetActualBraceTangents(brace, animSet);
    }
}

// Unit v5278: GetActualBraceParams (0x1003d4cc) and UpdatePylonStackStructures (0x1003d844).

// 0x1003d4cc: for every brace of the current coaster (a list linked by +0x64) asks GetAlignmentParamsSlow for each of its 0x58-byte
// records, then clears flag 0x800 unless 0x1000 is set. The result of the call goes unused (the original also loads a dead word).
void GetActualBraceParams() {
    sBraceWalkData* data = static_cast<sBraceWalkData*>(gCoasterCurrentData);
    for (sBraceWalkNode* node = data->mHead; node != nullptr; node = node->mNext) {
        sBraceWalkRecord* record = node->mList->mRecords;
        for (uint32_t i = 0; i < node->mList->mCount; ++i) {
            sCoasterOrientation orientation;
            float sum = node->mF6C + record->mF0C;
            GetAlignmentParamsSlow(static_cast<sCoasterTrackParams*>(gCoasterCurrentData), &record->mPoint, sum, &orientation, 1, nullptr);
            ++record;
        }
        if ((node->mFlags & 0x1000) == 0) node->mFlags &= 0xfffff7ffu;
    }
}

// TPCoaster.cpp, main app (unit w148): the coaster type's remaining loaders. CreateTrackSectionMesh 0x10036564 (196 bytes),
// SetupTrainType 0x1003a150 (260), SetupCarType 0x10039bc0 (1,376), LoadCoasterPylons 0x1003a9a8 (380), SetupPylonData 0x1003a28c
// (1,748), CreateCrossSectionSelectsFromSamFile 0x1003851c (552), SetCrossSectionSelectDefaults 0x100369d8 (200),
// OptimiseCrossSections 0x100388cc (460), RepackCrossSection 0x100387b4 (208), GetTempFixedRideParams 0x1003d5b0 (44).
//
// Host names: the listing's sCrossSection is sCoasterRecord here, sCSPoint is sCrossSectionPoint; the invented member names are in
// include/TPCoaster.h. Float compares: the original's fcmpo + bge/ble branches are taken on an unordered result too, so every
// select below is a C ternary on `<` or `>` (a NaN falls to the side the comment names). No fused multiply-add and no lfd occur in
// any of the ten listings, so the plain float expressions are exact.

// 0x1003d5b0: one static params record (the cell x and z, no turn, a pointer to a static pair {a, b}); the same record every call.
sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z) {
    static int32_t sPair[2];              // 0x102b5c08
    static sFixedRideParams sParams;      // 0x102b5bf8
    sPair[0] = a;
    sPair[1] = b;
    sParams.mX = x;
    sParams.mZ = z;
    sParams.mRotation = 0;
    sParams.mF0C = sPair;
    return &sParams;
}

// TPCoaster.cpp (main app), unit v5091: NewPylon (0x1003d610) and UpdatePylonStackMeshes (0x1003d958).

uint32_t gPylonStackSkip = 0;   // 0x102b68a0

// 0x1003d610: a pylon record, linked into the editor coaster's list (or made its preview pylon when flag 4 is set), placed by a point or by a
// cell. The fourth argument is never read. The Mac asks MReleaseAllocF for 0x48 bytes; the host record is bigger (native pointers), so it asks
// for sizeof.
sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    (void)unused;
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    sPylonRecord* pylon = static_cast<sPylonRecord*>(MReleaseAllocF(sizeof(sPylonRecord), 1));
    if ((flags & 4) != 0) {
        instance->mPreviewPylon = pylon;
    } else {
        if (instance->mPylons == nullptr) instance->mPylons = reinterpret_cast<sCoasterPylon*>(pylon);
        if (instance->mPylonTail != nullptr) instance->mPylonTail->mNext = pylon;
        pylon->mPrev = instance->mPylonTail;
        instance->mPylonTail = pylon;
        pylon->mNumber = instance->mF40;
        instance->mF40 = instance->mF40 + 1;
    }
    pylon->mFlags = flags;
    if (point != nullptr) {
        pylon->mX = point->x;
        pylon->mY = point->y;
        pylon->mZ = point->z;
        const float ten = 10.0f;
        pylon->mCellX = static_cast<uint32_t>(FloatToCell(point->x / ten));
        pylon->mCellZ = static_cast<uint32_t>(FloatToCell(point->z / ten));
        if (static_cast<int32_t>(pylon->mCellX) < 0) pylon->mCellX = 0;
        if (static_cast<int32_t>(pylon->mCellZ) < 0) pylon->mCellZ = 0;
        if (static_cast<int32_t>(pylon->mCellX) >= gHeightfieldWidth) pylon->mCellX = static_cast<uint32_t>(gHeightfieldWidth - 1);
        if (static_cast<int32_t>(pylon->mCellZ) >= gHeightfieldHeight) pylon->mCellZ = static_cast<uint32_t>(gHeightfieldHeight - 1);
    } else {
        pylon->mCellX = x;
        pylon->mCellZ = z;
        const float ten = 10.0f;
        pylon->mX = ten * static_cast<float>(x);
        pylon->mZ = ten * static_cast<float>(z);
        pylon->mY = GetRideRequiredYOffset(static_cast<int32_t>(x), static_cast<int32_t>(z), nullptr, 1, 1, 0, 0);
        pylon->mIndex = x + z * gHeightfieldHeader->mHField->mF18[0];
    }
    pylon->mInstance = gCoasterEditInstance;
    pylon->mControl = control;
    return pylon;
}

// 0x1003d844: for each pylon of the stack (count at +0x30, pointers from +0x34) sets or clears 0x100 (set unless last) and points its +0x90 and
// +0x74 words at the mesh areas of its kind.
void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon) {
    (void)control;
    if (gPylonStackSkip != 0) return;
    sPylonStackHead* stack = reinterpret_cast<sPylonStackHead*>(pylon);
    for (uint32_t i = 0; i < stack->mCount; ++i) {
        uint32_t count = stack->mCount;
        sStackPylon* p = stack->mPylons[i];
        if (count > 1 && i < count - 1)
            p->mFlags |= 0x100u;
        else
            p->mFlags &= 0xfffffeffu;
        sStackQ* q = p->mCtl->mQ;
        if ((p->mFlags & 0x2000) == 0 && (p->mFlags & 0x100) != 0 && (q->mFlags & 8) != 0) {
            p->mF90 = p->mArea84;
            p->mF74 = q->mArea44;
        } else {
            p->mF90 = p->mArea78;
            p->mF74 = q->mArea30;
        }
    }
}

namespace {

// The node (0xa0 bytes) or mini node (0x58) of the ride's mesh with the index the pylon control gives, past the mesh's first node.
uint32_t* StackNodeWord(tag_sRideAnimSet* set, uint32_t index) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t first = header->mF46;
    uint32_t count = header->mNodeCount;
    uint32_t i = index + first;
    if (i < count) return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + i * 0xa0);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) + (i - count) * 0x58);
}

void SetStackNode(tag_sRideAnimSet* set, uint32_t index) { *StackNodeWord(set, index) |= 0x80000010u; }
void ClearStackNode(tag_sRideAnimSet* set, uint32_t index) { *StackNodeWord(set, index) &= ~0x10u; }

}  // namespace

// 0x1003d958 (the pylon is a sLinkPylon / sPylonRecord; tag_sPylon is the typedef the header declared it with).
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylonArg, int32_t a, int32_t b) {
    sLinkPylon* pylon = reinterpret_cast<sLinkPylon*>(pylonArg);
    if (gPylonStackSkip != 0 && a == 0) return;
    tag_sTrackBrace** braces = pylon->mBraces;
    for (uint32_t i = 0; i < pylon->mCount; i++) {
        tag_sTrackBrace* brace = braces[i];
        if (b == 0) FlagCollisionUpdateTrackAroundBrace(brace);
        if ((brace->mFlags & 0x2000) != 0) continue;
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        if (i == 0) {
            float y = pylon->mY;
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader->mF78.get()) + 0x44) = y;
            AnimateRideSet(set, 1, 0);
            uint32_t* root = reinterpret_cast<uint32_t*>(set->mHeads->mRootNode);
            if (root != nullptr) {
                *root &= ~0x10u;
                const float skirt = *reinterpret_cast<const float*>(brace->mData->mF1C + 4);
                DoMeshSkirtDeform(set->mHeads, gHeightfieldHeader, skirt, 0);
            }
        } else {
            tag_sTrackBrace* before = braces[i - 1];
            float y;
            if ((before->mFlags & 0x2000) != 0) {
                y = *reinterpret_cast<const float*>(before->mF0C + (0x50 - 0x0c)) + pylon->mControl->mFC4;
            } else {
                tag_sRideAnimSet* beforeSet = gRideAnimSets[before->mWord70];
                sPylonHeadTable* table = static_cast<sPylonHeadTable*>(beforeSet->mHeads->mF28);
                sPylonHeadRec* rec = table->mEntries[control->mF18].mRec;
                y = rec != nullptr ? rec->mY : 0.0f;
            }
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader->mF78.get()) + 0x44) = y;
            AnimateRideSet(set, 1, 0);
            if ((control->mFlags & 0x10) == 0) {
                uint32_t* root = reinterpret_cast<uint32_t*>(set->mHeads->mRootNode);
                if (root != nullptr) *root |= 0x80000010u;
            }
        }
        if (i == pylon->mCount - 1) {
            if ((control->mFlags & 0x400) != 0) SetStackNode(set, control->mF1C);
            if ((control->mFlags & 0x800) != 0) ClearStackNode(set, control->mF20);
        } else {
            if ((control->mFlags & 0x400) != 0 && (braces[i + 1]->mFlags & 0x2000) == 0) ClearStackNode(set, control->mF1C);
            if ((control->mFlags & 0x800) != 0) SetStackNode(set, control->mF20);
        }
        if (i == 0) {
            if ((control->mFlags & 0x1000) != 0) SetStackNode(set, control->mF24);
            if ((control->mFlags & 0x2000) != 0) ClearStackNode(set, control->mF28);
        } else {
            if ((control->mFlags & 0x1000) != 0) ClearStackNode(set, control->mF24);
            if ((control->mFlags & 0x2000) != 0) SetStackNode(set, control->mF28);
        }
    }
}

namespace {
// One entry of a brace info block: 0x58 bytes on the Mac, the brace pointer at 0x10 (native on the host, the entry size is the same).
struct sBraceEntry {
    uint32_t mF00;                    // 0x00
    uint32_t mIndex;                  // 0x04  the data node index
    uint8_t mF08[8];                  // 0x08
    tag_sTrackBrace* mBrace;          // 0x10
    uint8_t mF18[0x58 - 0x18];        // 0x18
};
static_assert(sizeof(sBraceEntry) == 0x58, "entry size");
}  // namespace

// 0x1003df2c (212 bytes)
void SetupBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info, sBraceControl* control, sPylonControl* pylon) {
    info->mCount = control->mCount;
    info->mBlockA = MReleaseAllocF(info->mCount * 0x58, 1);
    info->mBlockB = MReleaseAllocF(info->mCount * 0x58, 1);
    sBraceEntry* a = static_cast<sBraceEntry*>(info->mBlockA);
    sBraceEntry* b = static_cast<sBraceEntry*>(info->mBlockB);
    tag_sRideAnimSet* set = gRideTypes[pylon->mRideType];
    for (uint32_t i = 0; i < info->mCount; i++) {
        a->mIndex = FindDataNodeIndexByID(set, 0x400000, i + control->mBase);
        b->mIndex = a->mIndex;
        a->mBrace = brace;
        a++;
        b->mBrace = brace;
        b++;
    }
}

// 0x1003e064 (92 bytes)
void DestroyBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info) {
    if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(info->mBlockA);
    MReleaseFreeF(info->mBlockB);
    info->mBlockA = nullptr;
    info->mBlockB = nullptr;
}

// Unit v5053: the loft of a new brace (0x1003e108) and the link level of a pylon (0x1003e7dc), TPCoaster.cpp on the Mac.

namespace {

// "TrackMaster" + 0x1fa, 0x237, 0x278, 0x2cf, 0x303, 0x33f
const char kLinkingOnTop[] = "Loop is linking ontop of pylon at level %d with height %.3f\n";
const char kMoreThanEnough[] = "More than enough room inside pylon at level %d with height %.3f\n";
const char kJustSqueeze[] = "Can just squeeze inside pylon at level %d with height %.3f - not returning yet though\n";
const char kStickingOnTop[] = "Sticking brace on top at level %d with height %.3f\n";
const char kGoingToSqueeze[] = "Going to squeeze inside pylon at level %d with height %.3f\n";
const char kCantFit[] = "Can't fit anywhere without moving track, setting valid at top, level %d with height %.3f\n";

// Pointers read at a Mac offset of an opaque block (the host's block holds a native pointer there, unaligned for the 4 byte offset).
template <class T>
T* PointerAt(const void* block, size_t offset) {
    T* value;
    std::memcpy(&value, static_cast<const uint8_t*>(block) + offset, sizeof(value));
    return value;
}

}  // namespace

float GetNewBraceLoftInterval(sLinkPylon* pylon, sPylonControl* control, tag_sTrackBrace* brace, tag_sTrackBrace* other, int32_t a, int32_t b,
                              float* out) {
    float height;   // f31
    if (control == nullptr || (control->mFlags & 0x4000) == 0 || a != 0) {
        height = gCoasterEditInstance->mF118;
    } else {
        s3DPoint from;
        s3DPoint to;
        from.x = pylon->mX;
        from.y = pylon->mY;
        from.z = pylon->mZ;
        to.x = pylon->mOther->mX;
        to.y = pylon->mOther->mY;
        to.z = pylon->mOther->mZ;
        from.y = 0.0f;
        to.y = 0.0f;
        float f0 = 0.0f;
        height = 0.0f;   // the original keeps its caller's f31 when `other` is NULL
        if (other != nullptr) {
            const sCoasterPylon* data = reinterpret_cast<const sCoasterPylon*>(other->mData);
            if (data->mFlags & 1) {
                std::memcpy(&f0, &other->mF0C[0x50 - 0x0c], sizeof(float));
            } else {
                sPylonControl* otherControl = data->mControl;
                const void* mesh = gRideAnimSets[other->mWord70]->mHeads;
                const void* holder = static_cast<const tag_sMesh*>(mesh)->mF28;
                const uint8_t* table = PointerAt<const uint8_t>(holder, 4);
                const uint8_t* entry = PointerAt<const uint8_t>(table + otherControl->mF2C * 0x14, 4);
                if (entry != nullptr) std::memcpy(&f0, entry + 0x34, sizeof(float));
            }
            height = f0;
        }
        float distance = DistanceBetweenPoints(&from, &to);
        height = ppc_fmadds(distance, 0.5f, height);
        const sCoasterPylon* braceData = reinterpret_cast<const sCoasterPylon*>(brace->mData);
        float interval = GetIntervalForHeight(height, braceData->mControl);
        SetLoftPylon(interval, brace->mNumber, 1, 0);
    }
    if (out != nullptr) *out = height;
    float result = GetIntervalForHeight(height, control);
    if (pylon->mCount == 0) {
        if (!(result >= 0.0f)) result = 0.0f;
        if (!(result <= 1.0f)) result = 1.0f;
    }
    if (b != 0) result = height;
    return result;
}

// 0x1003e344 (256 bytes)
void LinkPylon(sPylonRecord* pylon, sCoasterTrackParams* params, uint32_t x, uint32_t z, uint32_t index) {
    pylon->mFlags |= 8;
    pylon->mIndex = index;
    pylon->mCellX = x;
    pylon->mCellZ = z;
    float fx = static_cast<float>(x);
    float fz = static_cast<float>(z);
    pylon->mX = 10.0f * fx;
    pylon->mZ = 10.0f * fz;
    pylon->mY = GetRideRequiredYOffset(static_cast<int32_t>(x), static_cast<int32_t>(z), nullptr, 1, 1, 0, 0);
    pylon->mPrev = params->mPylonTail;
    pylon->mNext = params->mPylonTail->mNext;
    if (pylon->mPrev != nullptr) pylon->mPrev->mNext = pylon;
    if (pylon->mNext != nullptr) pylon->mNext->mPrev = pylon;
    params->mPylonTail = pylon;
    pylon->mNumber = params->mPylonCount;
    params->mPylonCount = params->mPylonCount + 1;
}

// 0x1003e48c (236 bytes)
void LinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params, int32_t valid) {
    if (valid != 0) {
        brace->mFlags &= ~0x4000u;
    } else {
        brace->mFlags |= 0x4000;
    }
    brace->mFlags |= 0x8000;
    brace->mNext = params->mBraceTail;
    brace->mPrev = params->mBraceTail->mPrev;
    if (brace->mNext != nullptr) brace->mNext->mPrev = brace;
    if (brace->mPrev != nullptr) brace->mPrev->mNext = brace;
    tag_sTrackBrace* tail = params->mBraceTail;
    if (params->mBraceHead == tail) {
        params->mBraceHead = brace;
    } else {
        uint32_t f = tail->mFlags;
        if ((f & 0x200) != 0) {
            tail->mFlags = f & ~0x10000u;
        } else {
            tail->mFlags = f & 0xfffaffffu;
        }
        brace->mFlags |= 0x50000;
    }
    params->mBraceTail = brace;
    params->mBraceCount = params->mBraceCount + 1;
    CorrectBraceNumbering(params->mBraceTail);
}

// 0x1003e5c0 (212 bytes)
void UnLinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params) {
    uint32_t f = brace->mFlags;
    if ((f & 0x8000) == 0) return;
    if (params->mBraceHead == params->mBraceTail) {
        params->mBraceHead = brace->mNext;
    } else {
        brace->mFlags = f & 0xfffaffffu;
        brace->mNext->mFlags |= 0x50000;
    }
    params->mBraceTail = brace->mNext;
    if (brace->mNext != nullptr) brace->mNext->mPrev = brace->mPrev;
    if (brace->mPrev != nullptr) brace->mPrev->mNext = brace->mNext;
    brace->mNext = nullptr;
    brace->mPrev = nullptr;
    brace->mNumber = 0;
    params->mBraceCount = params->mBraceCount - 1;
    CorrectBraceNumbering(params->mBraceTail);
    brace->mFlags &= ~0x8000u;
}

// Unit v5667: the height of a brace in its pylon's stack, squishing the stack, inserting a brace into a pylon and restoring a saved pylon
// (TPCoaster.cpp): CalculateBraceHeight 0x1003e6e0, RestorePylonDetails 0x1003ec00, SquishPylonBracesIfRequired 0x1003ee10,
// InsertBraceIntoPylon 0x1003ef78.
//
// The host's records differ from the Mac's: the pylon a brace points at is an sPylonRecord (native pointers), the brace's word at +4 (its
// number in the pylon) is read through mF00, and the words RestorePylonDetails copies at Mac offsets 0x94..0xac are appended fields.

namespace {

// The brace's number in its pylon (the Mac word at +4).
uint32_t& NumberOf(tag_sTrackBrace* brace) { return *reinterpret_cast<uint32_t*>(&brace->mF00[4]); }

// The pylon a brace stands in (the Mac word at +0x100).
sPylonRecord* PylonOf(const tag_sTrackBrace* brace) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(brace->mData));
}

// The Mac word at +0x50 (BraceY of the neighbouring units).
float& HeightOf(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x50); }

sCoasterType* TypeOf(tag_sCoasterInstance* instance) {
    return reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(instance->mType));
}

// The interval clamped to the two constants at 0x1029cd30 / 0x1029cd34 (0.0f and 1.0f; a NaN becomes the first, as the compares leave it).
float ClampInterval(float value) {
    // fcmpo then bge: the branch is taken for greater, equal and unordered, so a NaN passes both compares unchanged.
    if (value < 0.0f) value = 0.0f;
    if (1.0f < value) value = 1.0f;
    return value;
}

}  // namespace

float CalculateBraceHeight(tag_sTrackBrace* brace) {
    if (brace == nullptr) return 0.0f;
    sPylonRecord* pylon = PylonOf(brace);
    sPylonControl* control = pylon->mControl;
    float total = 0.0f;
    for (uint32_t i = 0; i < NumberOf(brace); i++) {
        tag_sTrackBrace* below = pylon->mBraces[i];
        if (below != nullptr) {
            float height = GetHeightForInterval(below->mFE8w, control);
            total = total + height;
            total = total + control->mFC4;
        }
    }
    float own = GetHeightForInterval(brace->mFE8w, control);
    return total + own;
}

int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet) {
    sPylonControl* control = reinterpret_cast<sPylonControl*>(pylon->mControl);
    float limit = height;
    if (control->mF6C < height) limit = control->mF6C;   // fVar1: the height capped at the control's top

    if ((gCoasterInputFlags & 0x100) == 0 && pylon->mCount != 0 && (pylon->mBraces[0]->mFlags & 0x40) != 0) {
        float top = CalculateBraceHeight(pylon->mBraces[pylon->mCount - 1]);
        float extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
        float sum = top + extra;
        float reach = control->mF68 + sum;
        if (reach < control->mF6C) {
            *level = pylon->mCount;
            extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
            float value = control->mF68 + (top + extra);
            *out = (value >= limit) ? value : limit;
            if (quiet == 0) CTRACE(5, kLinkingOnTop, *level, static_cast<double>(*out));
            return 1;
        }
    }

    bool found = false;
    uint32_t foundLevel = 0;
    float previous = 0.0f;   // f31
    for (uint32_t i = 0; i < pylon->mCount; i++) {
        tag_sTrackBrace* brace = pylon->mBraces[i];
        float step = i != 0 ? 1.0f : 0.0f;   // f29
        float current = CalculateBraceHeight(brace);
        float f4 = control->mFC4;
        float f5 = control->mF68;
        float need = ppc_fmadds(2.0f, f5, f4 * (1.0f + step));
        float room = 0.01f + (current - previous);
        if (room >= need) {
            float inside = 0.01f + ((current - f4) - f5);
            if (limit < inside) {
                uint32_t word;
                std::memcpy(&word, &brace->mF00[4], sizeof(word));
                *level = word;
                *out = limit;
                if (quiet == 0) CTRACE(5, kMoreThanEnough, *level, static_cast<double>(*out));
                return 1;
            }
            found = true;
            foundLevel = i;
            *out = f5 + ppc_fmadds(f4, step, previous);
            if (quiet == 0) CTRACE(5, kJustSqueeze, i, static_cast<double>(*out));
        }
        previous = current;
    }

    uint32_t count = pylon->mCount;
    float extra = count != 0 ? control->mFC4 : 0.0f;
    float reach = control->mF68 + (previous + extra);
    if (reach < control->mF6C) {
        *level = count;
        extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
        float value = control->mF68 + (previous + extra);
        *out = (value >= limit) ? value : limit;
        *out = (*out < control->mF6C) ? *out : control->mF6C;
        if (quiet == 0) CTRACE(5, kStickingOnTop, *level, static_cast<double>(*out));
        return 1;
    }
    if (found) {
        *level = foundLevel;
        if (quiet == 0) CTRACE(5, kGoingToSqueeze, *level, static_cast<double>(*out));
        return 1;
    }
    *level = count;
    *out = control->mF68 + (previous + control->mFC4);
    if (quiet == 0) CTRACE(5, kCantFit, *level, static_cast<double>(*out));
    return 1;
}

void RestorePylonDetails(tag_sCoasterInstance* instance, tag_sPylon* pylonArg) {
    sPylonRecord* pylon = reinterpret_cast<sPylonRecord*>(pylonArg);
    std::memcpy(pylon, &gLinkSavedPylon, sizeof(sPylonRecord));   // the Mac copies 0x48 bytes
    uint32_t i = 0;
    for (; i < gLinkSavedPylon.mCount; i++) {
        tag_sTrackBrace* brace = pylon->mBraces[i];
        tag_sTrackBrace* saved = &gLinkSavedBraces[i];
        NumberOf(brace) = NumberOf(saved);
        brace->mFlags = (brace->mFlags & 0x600000u) | saved->mFlags;
        std::memcpy(&brace->mF0C[0], &saved->mF0C[0], 0x58);   // 0x0c..0x63
        brace->mPrev = saved->mPrev;
        brace->mNext = saved->mNext;
        brace->mF6C = saved->mF6C;
        brace->mF90 = saved->mF90;
        brace->mF94w = saved->mF94w;
        brace->mW98 = saved->mW98;
        brace->mW9C = saved->mW9C;
        brace->mWA0 = saved->mWA0;
        brace->mWA4 = saved->mWA4;
        brace->mWA8 = saved->mWA8;
        brace->mWAC = saved->mWAC;
        brace->mFD4 = saved->mFD4;
        brace->mFD8 = saved->mFD8;
        brace->mFDC = saved->mFDC;
        brace->mAngle = saved->mAngle;
        brace->mFE4 = saved->mFE4;
        brace->mFE8w = saved->mFE8w;
        brace->mFEC = saved->mFEC;
        brace->mFF0 = saved->mFF0;
        brace->mFF4 = saved->mFF4;
        brace->mFF8 = saved->mFF8;
        brace->mSample = saved->mSample;
        brace->mData = saved->mData;
        HeightOf(brace) = gLinkSavedHeights[i];
        SetupBraceAnimFrameData(TypeOf(instance), brace);
    }
    for (; i < 5; i++) pylon->mBraces[i] = nullptr;
    gLinkSavedFlag = 0;
}

int32_t SquishPylonBracesIfRequired(tag_sCoasterInstance* instance, sPylonControl* control, sLinkPylon* pylon) {
    int32_t changed = 0;
    uint32_t count = pylon->mCount;
    if (count > 1) {
        tag_sTrackBrace* top = pylon->mBraces[count - 1];
        for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; i--) {
            tag_sTrackBrace* brace = pylon->mBraces[i];
            if (!(CalculateBraceHeight(top) > control->mF6C)) return changed;   // ble: also taken for a NaN
            float interval = GetIntervalForHeight(control->mF68, control);
            brace->mFE8w = ClampInterval(interval);
            SetupBraceAnimFrameData(TypeOf(instance), brace);
            changed = 1;
        }
    }
    return changed;
}

void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d) {
    sPylonControl* control = pylon->mControl;
    tag_sTrackBrace** braces = pylon->mBraces;
    tag_sTrackBrace* next;
    float nextHeight;
    float room;   // the stack height the next brace leaves, 0x48(r1)
    if (level < pylon->mCount) {
        next = braces[level];
        nextHeight = CalculateBraceHeight(next);
        float gap = control->mF68 + control->mFC4;
        room = nextHeight - gap;
    } else {
        nextHeight = 0.0f;
        next = nullptr;
        room = nextHeight;
    }
    float below;   // f2: the height under the new brace
    if (level != 0) {
        float under = CalculateBraceHeight(braces[level - 1]);
        below = control->mFC4 + under;
    } else {
        below = 0.0f;
    }
    if (b == 0) {
        for (uint32_t i = pylon->mCount; i > level; i--) {
            braces[i] = braces[i - 1];
            NumberOf(braces[i]) = i;
        }
    }
    braces[level] = brace;
    if (a != 0) pylon->mFlags |= 0x20;
    NumberOf(brace) = level;
    brace->mData = reinterpret_cast<const sTrackBraceData*>(pylon);
    pylon->mCount = pylon->mCount + 1;

    if (next == nullptr || b != 0) {
        if (control != nullptr) {
            float local = height;
            if (NumberOf(brace) != 0) {
                float under = CalculateBraceHeight(PylonOf(brace)->mBraces[NumberOf(brace) - 1]);
                local = local - (under + control->mFC4);
            }
            if (d == 0) {
                if (local < control->mF68) local = control->mF68;
                if (control->mF6C < local) local = control->mF6C;
            }
            float interval = GetIntervalForHeight(local, control);
            brace->mFE8w = ClampInterval(interval);
            SetupBraceAnimFrameData(TypeOf(instance), brace);
        }
        if (out != nullptr) *out = height;
    } else {
        if (!(height < room)) height = room;
        float local = height - below;
        if (local < control->mF68) local = control->mF68;
        float result = local + below;   // f30
        float interval = GetIntervalForHeight(local, control);
        brace->mFE8w = ClampInterval(interval);
        SetupBraceAnimFrameData(TypeOf(gCoasterEditInstance), brace);
        float nextLocal = nextHeight;
        if (NumberOf(next) != 0) {
            float under = CalculateBraceHeight(PylonOf(next)->mBraces[NumberOf(next) - 1]);
            nextLocal = nextLocal - (under + control->mFC4);
        }
        if (d == 0) {
            if (nextLocal < control->mF68) nextLocal = control->mF68;
            if (control->mF6C < nextLocal) nextLocal = control->mF6C;
        }
        float nextInterval = GetIntervalForHeight(nextLocal, control);
        next->mFE8w = ClampInterval(nextInterval);
        SetupBraceAnimFrameData(TypeOf(instance), next);
        if (out != nullptr) *out = result;
    }

    if (control != nullptr && b == 0) {
        if (c == 0) SquishPylonBracesIfRequired(instance, control, pylon);
        if (gPylonStackSkip == 0) {
            for (uint32_t i = 0; i < pylon->mCount; i++) {
                tag_sTrackBrace* stacked = braces[i];
                uint32_t count = pylon->mCount;
                if (count > 1 && i < count - 1)
                    stacked->mFlags |= 0x100;
                else
                    stacked->mFlags &= ~0x100u;
                sPylonControl* stackedControl = PylonOf(stacked)->mControl;
                if ((stacked->mFlags & 0x2000) == 0 && (stacked->mFlags & 0x100) != 0 && (stackedControl->mFlags & 8) != 0) {
                    stacked->mF90 = reinterpret_cast<sBraceSpline*>(&stacked->mCount84);
                    stacked->mF74 = reinterpret_cast<const uint32_t*>(&stackedControl->mF44);
                } else {
                    stacked->mF90 = reinterpret_cast<sBraceSpline*>(&stacked->mCount78);
                    stacked->mF74 = reinterpret_cast<const uint32_t*>(&stackedControl->mF30);
                }
            }
        }
        UpdatePylonStackMeshes(control, reinterpret_cast<tag_sPylon*>(pylon), 0, 0);
    }
}

// Unit v5366: the track editor's preview brace (TPCoaster.cpp on the Mac).
//   LinkPreviewBrace 0x1003f9fc (1,184 bytes), LinkPreviewBraceToPylon 0x1003f4fc (740 bytes)

// The copies LinkPreviewBraceToPylon keeps (the Mac's BSS words).
uint32_t gLinkSavedFlag;               // 0x102b6efc
sPylonRecord gLinkSavedPylon;          // 0x102b6eb4
tag_sTrackBrace gLinkSavedBraces[5];   // 0x102b69a0
float gLinkSavedHeights[5];            // 0x102b698c
sPylonRecord* gLinkFoundPylon;         // 0x102b6f00

namespace {

// The words of a brace the layout leaves inside a byte block (the same offsets on the Mac and the host).
float& BraceX(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x4c); }
float& BraceY(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x50); }
float& BraceZ(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x54); }

sCoasterTrackParams* ParamsOf(tag_sCoasterInstance* instance) { return reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C); }

}  // namespace

void LinkPreviewBraceToPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sPylonRecord* pylon, float unused, int32_t copy) {
    (void)unused;
    // r22: the brace of the drag (the second one when the block's flag 2 is set)
    tag_sTrackBrace* drag = (instance->mBlock3C & 2) != 0 ? instance->mBrace64 : instance->mLastBrace;
    sPylonControl* control;
    if (copy != 0) {
        std::memcpy(&gLinkSavedPylon, pylon, sizeof(sPylonRecord));
        for (uint32_t i = 0; i < gLinkSavedPylon.mCount; i++) {
            tag_sTrackBrace* source = pylon->mBraces[i];
            if (source != nullptr) std::memcpy(&gLinkSavedBraces[i], source, sizeof(tag_sTrackBrace));
            gLinkSavedHeights[i] = BraceY(source);
        }
        gLinkSavedFlag = 1;
        control = pylon->mControl;
        if ((gCoasterInputFlags & 0x40) != 0) {
            brace->mFlags |= 0x40;
            if (gCoasterWord1790 == 2) brace->mFlags |= 0x80;
        }
    } else {
        if ((gCoasterInputFlags & 0x40) != 0) {
            brace->mFlags |= 0x40;
            control = reinterpret_cast<sPylonControl*>(reinterpret_cast<char*>(const_cast<uint32_t*>(instance->mType)) + 0x1c8);
            if (gCoasterWord1790 == 2) brace->mFlags |= 0x80;
        } else {
            control = reinterpret_cast<sPylonControl*>(reinterpret_cast<char*>(const_cast<uint32_t*>(instance->mType)) + 0xa8);
        }
        pylon->mControl = control;
    }

    float height;
    bool reuse;
    if (copy != 0 && reinterpret_cast<const sPylonRecord*>(drag->mData) == pylon) {
        reuse = true;
        uint32_t index = *drag->mF74;
        const char* table = reinterpret_cast<const char*>(drag->mF90->mControlsA);
        height = *reinterpret_cast<const float*>(table + index * 0x58 + 0x44);
    } else {
        reuse = false;
        height = GetNewBraceLoftInterval(reinterpret_cast<sLinkPylon*>(pylon), control, brace->mNext, instance->mBraceB5C,
                                         static_cast<int32_t>(brace->mFlags & 0x80), 1, nullptr);
    }

    uint32_t level;
    float linkHeight;
    DetermineLinkLevel(height, reinterpret_cast<sLinkPylon*>(pylon), &level, &linkHeight, 1);
    BraceY(brace) = linkHeight;
    float inserted;
    InsertBraceIntoPylon(instance, brace, reinterpret_cast<sLinkPylon*>(pylon), level, linkHeight, &inserted, 1, 0, 1, 0);
    if (reuse) {
        BraceY(brace) = height;
    } else {
        BraceY(brace) = inserted;
        int32_t cellX = ppc_fctiwz(static_cast<double>(pylon->mX / 10.0f));
        int32_t cellZ = ppc_fctiwz(static_cast<double>(pylon->mZ / 10.0f));
        float offset = GetRideRequiredYOffset(cellX, cellZ, nullptr, 1, 1, 0x3fa, 0);
        BraceY(brace) = BraceY(brace) + offset;
    }
    brace->mFE8w = 0.0f;
    BraceX(brace) = ppc_fmadds(10.0f, static_cast<float>(pylon->mCellX), 5.0f);
    BraceZ(brace) = ppc_fmadds(10.0f, static_cast<float>(pylon->mCellZ), 5.0f);
}

// 0x1003f848 (216 bytes): the pylon of the cell under the pointer. The cell is the pointer's world x and z over 10
// (fctiwz); its key is x + z * the height field's row stride (word 0x18 of the height field). The instances are
// walked in list order, each one's pylon list (head at +0x4c, linked at +0x28); pylons with flag 1 or 4 are skipped.
tag_sPylon* GetPylonUnderCursor(uint32_t* a, uint32_t* b, uint32_t* c) {
    float fz = gPointerWorldPosn.z / 10.0f;
    float fx = gPointerWorldPosn.x / 10.0f;
    uint32_t stride = gHeightfieldHeader->mHField->mF18[0];
    uint32_t cellZ = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fz)));
    uint32_t cellX = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fx)));
    uint32_t key = cellX + cellZ * stride;

    sCoasterPylon* found = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sCoasterPylon* p = instance->mPylons;
        while (p != nullptr && ((p->mFlags & 5) != 0 || p->mF08 != key)) p = p->mNext;
        if (p != nullptr) {
            found = p;
            break;
        }
    }
    if (a != nullptr) *a = cellX;
    if (b != nullptr) *b = cellZ;
    if (c != nullptr) *c = key;
    return reinterpret_cast<tag_sPylon*>(found);
}

// Unit v5L20: the coaster collision system (TPConfig.cpp on the Mac): the boxes of the track between braces, the chunks of two tracks
// inside the boxes' overlap, the test of two chunks' cross sections, the braces' collision lists, and GetPylonAtCell.

// 0x1003f954 (124 bytes): the first pylon of any coaster on the cell (not a preview, flags 1 and 4 clear); the cell's index is
// written to `out` when given.
tag_sPylon* GetPylonAtCell(uint32_t x, uint32_t z, uint32_t* out) {
    const uint32_t index = x + z * gHeightfieldHeader->mHField->mF18[0];
    sPylonRecord* found = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sPylonRecord* node = reinterpret_cast<sPylonRecord*>(instance->mPylons);
        while (node != nullptr && ((node->mFlags & 5) != 0 || node->mIndex != index)) node = node->mNext;
        if (node != nullptr) {
            found = node;
            break;
        }
    }
    if (out != nullptr) *out = index;
    return reinterpret_cast<tag_sPylon*>(found);
}

void LinkPreviewBrace(tag_sCoasterInstance* instance) {
    sCoasterTrackParams* params = ParamsOf(instance);
    uint32_t flags = instance->mBlock3C;

    uint32_t cellZ = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f)));
    uint32_t cellX = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f)));
    uint32_t stride = gHeightfieldHeader->mHField->mF18[0];
    uint32_t index = cellX + cellZ * stride;

    sPylonRecord* found = nullptr;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) {
        sPylonRecord* p = reinterpret_cast<sPylonRecord*>(it->mPylons);
        while (p != nullptr && ((p->mFlags & 5) != 0 || p->mIndex != index)) p = p->mNext;
        if (p != nullptr) {
            found = p;
            break;
        }
    }
    gLinkFoundPylon = found;

    float dz = ppc_fnmsubs(10.0f, static_cast<float>(cellZ), BraceZ(instance->mPreviewBrace));
    float dx = ppc_fnmsubs(10.0f, static_cast<float>(cellX), BraceX(instance->mPreviewBrace));
    dz = dz * dz;
    float distance = ppc_fmadds(dx, dx, dz);
    if (distance > 90000.0f) return;

    int32_t valid = IsValidPylonPosition(cellX, cellZ, nullptr, 0, 0, 0, 0);
    float cost;
    if (found == nullptr) {
        LinkPylon(instance->mPreviewPylon, params, cellX, cellZ, index);
        LinkBrace(instance->mBrace6C, params, valid);
        LinkPreviewBraceToPylon(instance, instance->mBrace6C, instance->mPreviewPylon, instance->mF118, 0);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        CorrectBraceNumbering(instance->mBrace6C);
    } else {
        uint32_t two = flags & 2;
        if (two != 0) {
            bool hit = false;
            for (uint32_t i = 0; i < found->mCount; i++) {
                if (two != 0 && found->mBraces[i] == instance->mBrace64) {
                    hit = true;
                    break;
                }
                if (two == 0 && found->mBraces[i] == instance->mLastBrace) {
                    hit = true;
                    break;
                }
            }
            if (hit) {
                instance->mFlags |= 8;
                instance->mPreviewBrace->mFlags &= 0xfffaffffu;
                instance->mBrace64->mFlags |= 0x800000;
                if (valid != 0)
                    instance->mBrace64->mFlags |= 0x800000;
                else
                    instance->mBrace64->mFlags |= 0x804000;
                instance->mBrace64->mFlags &= 0xfff5ffffu;
                FlagCollisionUpdateTrackAroundBrace(instance->mPreviewBrace);
                FlagCollisionUpdateTrackAroundBrace(instance->mBrace64);
                return;
            }
        }
        uint32_t count = found->mCount;
        bool hit = false;
        for (uint32_t i = 0; i < found->mCount; i++) {
            if ((instance->mBlock3C & 2) != 0 && found->mBraces[i] == instance->mPreviewBrace) {
                hit = true;
                break;
            }
            if ((instance->mBlock3C & 2) == 0 && found->mBraces[i] == instance->mBraces) {
                hit = true;
                break;
            }
            count = found->mCount;
        }
        if (hit) return;
        count = found->mCount;
        uint32_t limit = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(found->mControl) + 0xcc);
        if (count >= limit) return;
        uint32_t input = gCoasterInputFlags & 0x40;
        uint32_t controlFlags = *reinterpret_cast<const uint32_t*>(found->mControl) & 0x4000;
        if (!(input != 0 && controlFlags != 0)) {
            if (input != 0) return;
            if (controlFlags != 0) return;
        }
        LinkBrace(instance->mBrace6C, params, valid);
        LinkPreviewBraceToPylon(instance, instance->mBrace6C, found, instance->mF118, 1);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        CorrectBraceNumbering(instance->mBrace6C);
    }

    tag_sTrackBrace* brace = instance->mBrace6C;
    if (brace->mData != nullptr) {
        cost = CalculateBraceCost(instance, 1, brace, &BraceX(brace), &BraceZ(brace), nullptr, nullptr);
    } else {
        cost = 0.0f;
    }
    if (gActionRecorder.mRecording == 0) {
        int32_t whole = ppc_fctiwz(static_cast<double>(cost));
        if (whole < 0)
            LLCC_SystemAddToSessionCost(whole - 0xeeeeeee);
        else
            LLCC_SystemAddToSessionCost(whole);
    }
}

// Unit v5157: the track editor's new brace and the undoing of its preview (TPCoaster.cpp; main app).
//   NewTrackBrace 0x10040304 (1,532 bytes), UnLinkPreviewBrace 0x1003fedc (1,000 bytes)
// The pylon records are sPylonRecords (native pointers, host offsets in the header); the instance's +0x3c block is its mBlock3C word.

namespace {

// 0x1029ccbc: the animation kind of each of the five time controls (flag bits 0x20 << i of the pylon control).
const uint32_t kAnimKinds[5] = {3, 10, 2, 9, 10};
// 0x1029ccd0: its index in the kind.
const uint32_t kAnimIndexes[5] = {0, 0, 0, 0, 1};

inline sPylonRecord* RecordOf(const sTrackBraceData* data) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(data));
}

}  // namespace

// 0x10040304: a new brace on the pylon; the three floats are the x, z and loft of a single-brace pylon (pylon flag 1). Flag bits: 1 restoring,
// 2 the second (preview) brace, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100 set brace flags, 0x200 assign the object to the AI world.
tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t flags, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra) {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    uint32_t* params = &instance->mBlock3C;
    // The Mac asks for 0x104 bytes; the host brace is larger (native pointers and the collision list), so ask for its size.
    tag_sTrackBrace* brace = static_cast<tag_sTrackBrace*>(MReleaseAllocF(sizeof(tag_sTrackBrace), 1));
    uint32_t second = flags & 2;
    if (second != 0) brace->mFlags |= 0x2000;
    if ((control == nullptr || (control->mFlags & 0x4000) == 0) && second == 0) instance->mBraceB5C = brace;
    if ((*params & 2) != 0) {
        instance->mPreviewBrace->mFlags &= 0xfffaffffu;
        brace->mFlags |= 0x50000;
        brace->mNext = instance->mPreviewBrace;
        brace->mPrev = instance->mPreviewBrace->mPrev;
        brace->mNext->mPrev = brace;
        brace->mPrev->mNext = brace;
        instance->mPreviewBrace = brace;
    } else if (second != 0) {
        instance->mBrace6C = brace;
    } else {
        if (instance->mLastBrace == nullptr) instance->mLastBrace = brace;
        if (instance->mBraces != nullptr) instance->mBraces->mPrev = brace;
        brace->mNext = instance->mBraces;
        instance->mBraces = brace;
        instance->mPreviewBrace = brace;
    }
    if (second == 0) {
        brace->mNumber = instance->mBraceCount;
        brace->mF6C = static_cast<float>(instance->mBraceCount);
    }
    if ((*params & 2) != 0) CorrectBraceNumbering(brace);
    if (control != nullptr && (control->mFlags & 0x4000) != 0) {
        if (loops == 0)
            brace->mFlags |= 0xe0;
        else
            brace->mFlags |= 0x60;
    }
    if ((pylon->mFlags & 1) != 0) {
        brace->mX4C = x;
        brace->mZ54 = z;
        brace->mL50 = loft;
        brace->mCount78 = 1;
        brace->mBlk7C = reinterpret_cast<char*>(brace) + 0xc;
        brace->mBlk80 = MReleaseAllocF(0x58, 1);
        brace->mCount84 = 1;
        brace->mBlk88 = brace->mBlk7C;
        brace->mBlk8C = brace->mBlk80;
        brace->mF90 = reinterpret_cast<sBraceSpline*>(&brace->mCount78);
        // The first info's two blocks each hold the brace at +0x10.
        *reinterpret_cast<tag_sTrackBrace**>(static_cast<char*>(brace->mBlk7C) + 0x10) = brace;
        *reinterpret_cast<tag_sTrackBrace**>(static_cast<char*>(brace->mBlk80) + 0x10) = brace;
    } else {
        if (control != nullptr && (control->mFlags & 0x4000) != 0 && loops != 0)
            brace->mFF8 = 0.5f;
        else
            brace->mFF8 = 0.0f;
        sFixedRideParams* ride = GetTempFixedRideParams(control->mFB4, control->mFB8, static_cast<int32_t>(pylon->mCellX),
                                                        static_cast<int32_t>(pylon->mCellZ));
        if ((flags & 1) == 0) {
            brace->mWord70 = InstanceRide(control->mRideType, 0x3fa, ride, 0, 0, nullptr, 0, 0, 0xffffffffu, 0);
        } else {
            brace->mWord70 = RestoreRideInstance(extra, control->mRideType, 0x3fa, ride, 0, nullptr, 0, 0, 0);
        }
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        set->mFlags |= 0x80000;
        set->mBraceBlock = brace;
        set = gRideAnimSets[brace->mWord70];
        char* timeControl = static_cast<char*>(set->mTimeControls);
        for (uint32_t i = 0; i < 5; i++) {
            if ((control->mFlags & (0x20u << i)) != 0) {
                uint32_t kind = kAnimKinds[i];
                if ((set->mGroups[kind].mFlags & 3) != 0)
                    SetNewAnim(set, kind, kAnimIndexes[i], reinterpret_cast<sAnimTimeControl*>(timeControl), 0, 1.0f);
            }
            timeControl += sizeof(sAnimTimeControl);
        }
        set->mFlags |= 0x1000;
        float value = brace->mFE8w * control->mFD0[2];
        value = value + control->mFD0[3];
        value = value + 0.0f;
        if (value < control->mFD0[0]) value = control->mFD0[0];
        if (control->mFD0[1] < value) value = control->mFD0[1];
        sAnimTimeControl* first = static_cast<sAnimTimeControl*>(gRideAnimSets[brace->mWord70]->mTimeControls);
        first->mF20 = value * first->mF1C;
        if ((control->mFlags & 8) != 0) {
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount78), reinterpret_cast<sBraceControl*>(&control->mF30), control);
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount84), reinterpret_cast<sBraceControl*>(&control->mF44), control);
        } else {
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount78), reinterpret_cast<sBraceControl*>(&control->mF30), control);
        }
        if ((flags & 6) == 0) *params |= 4;
    }
    if ((flags & 8) != 0) brace->mFlags |= 0x200;
    uint32_t bit10 = flags & 0x10;
    if (bit10 != 0) brace->mFlags |= 0x400;
    if ((flags & 0x20) != 0) brace->mFlags |= 0x1800;
    if ((flags & 0x40) != 0) brace->mFlags |= 0x80000;
    if ((flags & 0x80) != 0) brace->mFlags |= 0x40000;
    if ((flags & 0x100) != 0) brace->mFlags |= 0x100000;
    if ((flags & 0x200) != 0 && (flags & 1) == 0)
        Assign3DObjectToAIWorld(gCoasterEditInstance->mCObject, pylon->mCellX, pylon->mCellZ, brace->mWord70, static_cast<int32_t>(bit10));
    if (second == 0) {
        uint32_t four = flags & 4;
        InsertBraceIntoPylon(gCoasterEditInstance, brace, reinterpret_cast<sLinkPylon*>(pylon), level, loft, nullptr, 0,
                             static_cast<int32_t>(flags & 1), 0, static_cast<int32_t>(four));
        if (four == 0) {
            sPylonControl* pylonControl = pylon->mControl;
            if (pylonControl != nullptr && (pylonControl->mFlags & 0x4000) == 0) gCoasterEditInstance->mF118 = loft;
        }
        instance->mBraceCount = instance->mBraceCount + 1;
        if (instance->mBraceCount >= 2) instance->mF48 = instance->mF48 + 1.0f;
    }
    if ((flags & 1) == 0) FlagCollisionUpdateTrackAroundBrace(brace);
    return brace;
}

namespace {

// The common end of the three ways that take the brace out of the pylon: the brace is unlinked from the track, its pylon loses it, the
// pylon's braces are re-pointed and the stack meshes rebuilt, the brace is cleared and the next brace renumbered.
void UnLinkEnd(tag_sCoasterInstance* instance, tag_sTrackBrace* next) {
    UnLinkBrace(instance->mBrace6C, reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C));
    tag_sTrackBrace* brace = instance->mBrace6C;
    sPylonRecord* pylon = RecordOf(brace->mData);
    if (pylon != nullptr) {
        if (pylon->mCount < 2) {
            // The Mac clears the word at pylon + 0x30 + count * 4: the last brace slot, or the count itself when it is 0.
            if (pylon->mCount == 0)
                pylon->mCount = 0;
            else
                pylon->mBraces[0] = nullptr;
            pylon->mCount = pylon->mCount - 1;
        } else {
            RestorePylonDetails(instance, reinterpret_cast<tag_sPylon*>(pylon));
        }
        if (gPylonStackSkip == 0) {
            for (uint32_t i = 0; i < pylon->mCount; i++) {
                tag_sTrackBrace* member = pylon->mBraces[i];
                uint32_t count = pylon->mCount;
                if (count > 1 && i < count - 1)
                    member->mFlags |= 0x100;
                else
                    member->mFlags &= 0xfffffeffu;
                sPylonRecord* memberPylon = RecordOf(member->mData);
                if ((member->mFlags & 0x2000) == 0 && (member->mFlags & 0x100) != 0 && (memberPylon->mControl->mFlags & 8) != 0) {
                    member->mF90 = reinterpret_cast<sBraceSpline*>(&member->mCount84);
                    member->mF74 = reinterpret_cast<const uint32_t*>(&memberPylon->mControl->mF44);
                } else {
                    member->mF90 = reinterpret_cast<sBraceSpline*>(&member->mCount78);
                    member->mF74 = reinterpret_cast<const uint32_t*>(&memberPylon->mControl->mF30);
                }
            }
        }
        UpdatePylonStackMeshes(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon), 0, 0);
    }
    brace->mFlags &= 0xffffff3fu;
    brace->mData = nullptr;
    CorrectBraceNumbering(next);
}

}  // namespace

// 0x1003fedc: takes the editor's preview brace (mBrace6C) back out of its pylon and the track.
void UnLinkPreviewBrace(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* next = instance->mBrace6C->mNext;
    if (gLinkFoundPylon == nullptr) {
        if ((instance->mBrace6C->mFlags & 0x8000) != 0) {
            RemoveBraceFromCollisionSystem(instance->mBrace6C, instance);
            FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        }
        sPylonRecord* preview = instance->mPreviewPylon;
        if ((preview->mFlags & 8) != 0) {
            if (preview->mPrev != nullptr) preview->mPrev->mNext = nullptr;
            instance->mPylonTail = preview->mPrev;
            preview->mPrev = nullptr;
            preview->mNext = nullptr;
            preview->mNumber = 0;
            instance->mF40 = instance->mF40 - 1;
            preview->mFlags &= 0xfffffff7u;
        }
        UnLinkEnd(instance, next);
    } else if ((instance->mFlags & 8) == 0) {
        if ((instance->mBrace6C->mFlags & 0x8000) != 0) {
            RemoveBraceFromCollisionSystem(instance->mBrace6C, instance);
            FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        }
        UnLinkEnd(instance, next);
    } else {
        instance->mFlags &= 0xfffffff7u;
        instance->mPreviewBrace->mFlags |= 0x50000;
        instance->mBrace64->mFlags &= 0xff7fbfffu;
        instance->mBrace64->mFlags |= 0xa0000;
        FlagCollisionUpdateTrackAroundBrace(instance->mPreviewBrace);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace64);
    }
}

// DestroyTrackBrace 0x1004094c (308 bytes) and PlaceEntrancePylons 0x10040acc (764 bytes) of TPCoaster.cpp.

// 0x1004094c: frees a brace's blocks and the brace; the pylon data's flag 1 keeps the shared 0x7c and 0x88 blocks.
void DestroyTrackBrace(tag_sTrackBrace* brace, sPylonControl* pylon) {
    if (brace->mWord70 != 0) {
        DestroyRideInstance(brace->mWord70);
        brace->mWord70 = 0;
    }
    if (pylon != nullptr && (pylon->mFlags & 8) != 0) {
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk7C);
        MReleaseFreeF(brace->mBlk80);
        brace->mBlk7C = nullptr;
        brace->mBlk80 = nullptr;
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk88);
        MReleaseFreeF(brace->mBlk8C);
        brace->mBlk88 = nullptr;
        brace->mBlk8C = nullptr;
    } else {
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk7C);
        MReleaseFreeF(brace->mBlk80);
        brace->mBlk7C = nullptr;
        brace->mBlk80 = nullptr;
        brace->mBlk88 = nullptr;
        brace->mBlk8C = nullptr;
    }
    if (GetCurSelectedTrackBrace() == brace) SetCurSelectedTrackBrace(nullptr);
    MReleaseFreeF(brace);
}

// 0x10040acc: the entrance's two pylons (three with hoardings) and their braces, then the end pylon; nothing is made when the instance is the editor's (mF40).
void PlaceEntrancePylons(tag_sCoasterInstance* instance, uint32_t x, uint32_t z, float height, int32_t flag) {
    (void)x;
    (void)z;
    (void)height;
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    int32_t count, first;
    if ((type->mFlags & 0x20) != 0) {
        count = 3;
        first = 1;
    } else {
        count = 2;
        first = 2;
    }
    if (instance->mF40 == 0) {
        sPylonControl* control = &type->mPylonsA;
        uint32_t flags = 0;
        float local[3];
        for (int32_t i = 0; i < count; ++i) {
            tag_sRideAnimSet* set = gRideAnimSets[instance->mEntranceID];
            const uint32_t index = FindDataNodeIndexByID(set, 0x400000, static_cast<uint32_t>(i + first));
            if (index == 0xffffffffu) continue;
            set = gRideAnimSets[instance->mEntranceID];
            const sEmbeddedSet* table = static_cast<const sEmbeddedSet*>(set->mHeads->mF28);
            const uint8_t* block = table->mRecords[index].mBlock;
            if (block != nullptr) {
                const float* p = reinterpret_cast<const float*>(block + 0x30);
                local[0] = p[0];
                local[1] = p[1];
                local[2] = p[2];
            } else {
                local[0] = local[1] = local[2] = 0.0f;
            }
            if ((type->mFlags & 0x20) != 0 && (i == 0 || i == 2)) {
                set = gRideAnimSets[instance->mEntranceID];
                const uint8_t* node = reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get());
                local[1] = local[1] - *reinterpret_cast<const float*>(node + 0x44);
                const float ten = 10.0f;
                const uint32_t cellX = static_cast<uint32_t>(FloatToCell(static_cast<double>(local[0] / ten)));
                const uint32_t cellZ = static_cast<uint32_t>(FloatToCell(static_cast<double>(local[2] / ten)));
                sPylonRecord* pylon = NewPylon(cellX, cellZ, 0x12, 0, control, nullptr);
                if (i == 0)
                    flags = 0x274;
                else if (i == 2)
                    flags = 0x28c;
                if (flag != 0) flags |= 1;
                tag_sTrackBrace* brace = NewTrackBrace(pylon, 0.0f, 0.0f, local[1], flags, control, 0, 0, 0);
                SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), brace);
            } else {
                sPylonRecord* pylon = NewPylon(0, 0, 0x13, 0, nullptr, reinterpret_cast<s3DPoint*>(local));
                flags = flag != 0 ? 1 : 0;
                if ((type->mFlags & 0x20) != 0) flags |= 0x3c;
                NewTrackBrace(pylon, pylon->mX, pylon->mZ, 0.0f + pylon->mY, flags, nullptr, 0, 0, 0);
            }
        }
        local[0] = 0.0f;
        local[1] = 0.0f;
        local[2] = 0.0f;
        sPylonRecord* pylon = NewPylon(0, 0, 7, 0, nullptr, reinterpret_cast<s3DPoint*>(local));
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, 0.0f + pylon->mY, 2, nullptr, 0, 0, 0);
    }
    SetPotentialPylonArea();
    FlashEndPylons(instance, 1);
    FlagGenerateTrack(instance, 1);
}

// Coaster instances (TPCoaster.cpp on the Mac, unit w186): SetCurrentEditCoaster 0x1003aca0, the pylon interval three 0x10040e10, 0x10040e64 and
// 0x10040eb8, FlagGenerateTrack 0x10040f08, FlagCollisionUpdateTrackAroundBrace 0x10040fa4, XZAngleBetweenPylons 0x10041018,
// GetPotentialPylonPositionArea 0x10041170, DestroyEndPylons 0x10042728, EndTrack 0x100427d0, SetCoasterState 0x10042bb8, IsCoasterType 0x100432dc,
// IsCoaster 0x10043330, FindCoasterInstanceFromEntranceID 0x10043380, SetCoasterCObject 0x10043480, InstanceCoaster 0x100434ec,
// SetCoasterScriptID 0x1004373c and ui_GetNumCoasterTracks 0x100458ac. The globals' places are in include/TPCoaster.h (names invented); every
// callee outside the unit is untranslated.

uint32_t gCoasterWord689C = 0;                       // 0x102b689c
uint32_t gCoasterWord6898 = 0;                       // 0x102b6898
uint32_t gCoasterWord6894 = 0;                       // 0x102b6894
uint32_t gCoasterWord6890 = 0;                       // 0x102b6890

// 0x10040e10 (24 bytes): the place of the height between the pylon's two entries (single precision steps).
float GetIntervalForHeight(float height, sPylonControl* pylon) {
    const float below = height - pylon->mFBC;
    const float span = pylon->mFC0 - pylon->mFBC;
    return below / span;
}

// 0x10040e64 (20 bytes).
float GetIntervalForHeightDelta(float height, sPylonControl* pylon) {
    const float span = pylon->mFC0 - pylon->mFBC;
    return height / span;
}

// 0x10040eb8 (20 bytes): fmadds.
float GetHeightForInterval(float interval, sPylonControl* pylon) {
    const float span = pylon->mFC0 - pylon->mFBC;
    return ppc_fmadds(interval, span, pylon->mFBC);
}

// 0x10040f08 (92 bytes).
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) {
    RecalcTrackParams(instance);
    instance->mFlags |= 1;
    if (flag != 0) instance->mFlags |= 0x1000;
}

// 0x10040fa4 (40 bytes).
void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) {
    brace->mFlags |= 0x200000;
    if (brace->mNext != nullptr) brace->mNext->mFlags |= 0x200000;
}

// 0x10041018 (288 bytes): the angle at the second point between the directions to the first and the third in the x,z plane. The points are
// converted from the words (fcfid-style conversion to single), the lengths come from a fused multiply-add in double, the clamp lets NaN through.
float XZAngleBetweenPylons(uint32_t x1, uint32_t z1, uint32_t x2, uint32_t z2, uint32_t x3, uint32_t z3) {
    const float dz1 = static_cast<float>(z1) - static_cast<float>(z2);
    const float dx1 = static_cast<float>(x1) - static_cast<float>(x2);
    const float dx2 = static_cast<float>(x3) - static_cast<float>(x2);
    const float dz2 = static_cast<float>(z3) - static_cast<float>(z2);
    const float length1 = static_cast<float>(std::sqrt(ppc_fmadd(dx1, dx1, static_cast<double>(dz1 * dz1))));
    const float dx1n = dx1 / length1;
    const float dz1n = dz1 / length1;
    const float length2 = static_cast<float>(std::sqrt(ppc_fmadd(dx2, dx2, static_cast<double>(dz2 * dz2))));
    const float dx2n = dx2 / length2;
    const float dz2n = dz2 / length2;
    float dot = ppc_fmadds(dx1n, dx2n, dz1n * dz2n);
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    return static_cast<float>(std::acos(static_cast<double>(dot)));
}

// 0x10041170 (52 bytes): the four words, each answer from its own global.
void GetPotentialPylonPositionArea(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a = gCoasterWord689C;
    *b = gCoasterWord6898;
    *c = gCoasterWord6894;
    *d = gCoasterWord6890;
}

// The clamp bounds of the angle fraction (new globals, initialised data of the Mac).
float gPylonClampMax = 1.0f;   // 0x1029cd48
float gPylonClampMin = 0.0f;   // 0x1029cd4c

namespace {

// The brace's pylon record (mData is the Mac's word +0x100, a pylon: sPylonRecord on the host).
inline sPylonRecord* PylonOf_2(const tag_sTrackBrace* brace) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(brace->mData));
}

// The coaster type's control at +0xa8 (A) or +0x1c8 (B).
inline sPylonControl* ControlOf(const tag_sCoasterInstance* instance, bool second) {
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    return second ? &type->mPylonsB : &type->mPylonsA;
}

}  // namespace

// 0x100411e4 (2108 bytes): whether a pylon may stand on the cell (x, z). With `a` (a brace) the cells b, c are the brace's own and d, e the next
// brace's; without it the editor's braces decide. The registers the original leaves uninitialised (the next brace's cell when the brace has no
// next one) are zero here.
int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    tag_sCoasterInstance* edit = gCoasterEditInstance;
    int32_t valid = 1;
    tag_sTrackBrace* cand = nullptr;                          // r25
    tag_sTrackBrace* cur = static_cast<tag_sTrackBrace*>(a);  // r26
    uint32_t px = b, pz = c, qx = d, qz = e;                  // r5, r0, r11, r10

    if (a == nullptr) {
        px = pz = qx = qz = 0;
        if (edit->mBlock3C & 2) {
            if (gCoasterInputFlags & 0x100) {
                tag_sTrackBrace* mv = gPylonMoveBrace;
                tag_sTrackBrace* nx = mv->mNext;
                cand = mv;
                if (nx->mFlags & 0x10000) cur = nullptr;
                else cur = nx;
            } else {
                cur = edit->mPreviewBrace;
            }
        } else {
            if (gCoasterInputFlags & 0x100) {
                cand = gPylonMoveBrace;
                cur = cand->mNext;
            } else {
                cur = edit->mBraces;
            }
        }
        if (cur != nullptr) {
            px = PylonOf_2(cur)->mCellX;
            pz = PylonOf_2(cur)->mCellZ;
            tag_sTrackBrace* nx = cur->mNext;
            if (nx != nullptr) {
                qx = PylonOf_2(nx)->mCellX;
                qz = PylonOf_2(nx)->mCellZ;
            }
        }
    }

    sHField* field = gHeightfieldHeader->mHField;
    uint32_t width = field->mF18[0];
    if (x >= width || z >= field->mF18[1]) return 0;
    if (a == nullptr) {
        if (gHeightfieldCopy->mQuads[x + width * z].mF00 & 1) return 0;
    }

    const uint32_t in = gCoasterInputFlags;
    const bool in100 = (in & 0x100) != 0;
    tag_sTrackBrace* second = nullptr;   // r3
    tag_sTrackBrace* prev = nullptr;     // r20
    if (in100 || a != nullptr) {
        if (cur != nullptr) {
            prev = cur->mPrev;
            if (prev == nullptr) prev = edit->mLastBrace->mPrev;
            second = cur->mNext;
            if (second == nullptr) second = edit->mBraces;
        }
    } else {
        second = cur != nullptr ? cur->mNext : nullptr;
    }

    if (second != nullptr && in100) {
        uint32_t f = second->mFlags;
        if ((f & 0x10000) || (cur->mFlags & 0x10000) || (prev->mFlags & 0x10000)) {
            if ((f & 0x20000) || (cur->mFlags & 0x20000) || (prev->mFlags & 0x20000)) second = nullptr;
        }
    }

    sPylonControl* ctl;
    if (in100 || a != nullptr) {
        if (cur != nullptr) ctl = ControlOf(edit, (cur->mFlags & 0x40) != 0 || (in & 0x40) != 0);
        else ctl = nullptr;
    } else {
        ctl = ControlOf(edit, (edit->mPreviewBrace->mFlags & 0x40) != 0 || (in & 0x40) != 0);
    }

    if (cur != nullptr) {
        if (px == x && pz == z) {
            valid = 0;
        } else {
            uint32_t dx = px - x;
            uint32_t dz = pz - z;
            uint32_t d2 = dx * dx + dz * dz;
            bool bad = false;
            if (d2 < static_cast<uint32_t>(ctl->mF9C) || d2 > static_cast<uint32_t>(ctl->mFA0)) {
                if (cur->mNumber == 2 || (PylonOf_2(cur)->mFlags & 2) == 0) bad = true;
            }
            if (bad) {
                valid = 0;
            } else if (second != nullptr) {
                float angle = std::fabs(XZAngleBetweenPylons(x, z, px, pz, qx, qz));
                float root = static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(d2))));
                float t = (root - ctl->mFA4) / (ctl->mFA8 - ctl->mFA4);
                // bge / ble after fcmpo are "not less" / "not greater": taken when unordered, so a NaN passes through.
                t = (gPylonClampMax < t) ? gPylonClampMax : t;
                t = (t < gPylonClampMin) ? gPylonClampMin : t;
                const float inv = 1.0f - t;
                float lo, hi;
                const float* ang = ctl->mAngles;
                if (PylonOf_2(cur)->mFlags & 2) {
                    lo = ppc_fmadds(ang[0], inv, ang[2] * t);
                    hi = ppc_fmadds(ang[1], inv, ang[3] * t);
                } else {
                    lo = ppc_fmadds(ang[4], inv, ang[6] * t);
                    hi = ppc_fmadds(ang[5], inv, ang[7] * t);
                }
                if (angle < lo || angle > hi) valid = 0;
            }
        }
    }

    if (in100 || a != nullptr) {
        if (cur != nullptr) ctl = reinterpret_cast<sPylonControl*>(PylonOf_2(prev)->mControl);
        else ctl = PylonOf_2(gPylonMoveBrace)->mControl;
    } else {
        ctl = ControlOf(edit, (in & 0x40) != 0);
    }

    if (valid != 0) {
        const uint32_t cell = x + z * width;
        sPylonRecord* found = nullptr;
        for (tag_sCoasterInstance* inst = gCoasterInstances; inst != nullptr && found == nullptr; inst = inst->mNext) {
            sPylonRecord* node = reinterpret_cast<sPylonRecord*>(inst->mPylons);
            while (node != nullptr && ((node->mFlags & 5) != 0 || node->mIndex != cell)) node = node->mNext;
            found = node;
        }
        if (in100) {
            if (found != nullptr && a == nullptr) {
                tag_sTrackBrace* mv = gPylonMoveBrace;
                if (reinterpret_cast<tag_sCoasterInstance*>(found->mInstance) != edit) {
                    valid = 0;
                } else if (found->mControl != ctl) {
                    valid = 0;
                } else if (mv != nullptr && PylonOf_2(mv) == found && found->mCount > static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if ((mv == nullptr || PylonOf_2(mv) != found) && found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x600) {
                    valid = 0;
                }
            }
        } else if (found != nullptr && a == nullptr) {
            if (reinterpret_cast<tag_sCoasterInstance*>(found->mInstance) != edit) {
                valid = 0;
            } else if (edit->mBlock3C & 2) {
                tag_sTrackBrace* br = edit->mBrace64;
                if (PylonOf_2(br)->mIndex == cell) {
                    if (gCoasterWord1790 == 1) valid = 0;
                    cand = br;
                } else if (found->mControl != ctl) {
                    valid = 0;
                } else if (found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x600) {
                    valid = 0;
                }
            } else {
                if ((edit->mBlock3C & 1) == 0) {
                    tag_sTrackBrace* br = edit->mLastBrace;
                    if (cell == PylonOf_2(br)->mIndex) cand = br;
                }
                if (found->mControl != ctl) {
                    valid = 0;
                } else if (found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x200) {
                    valid = 0;
                }
            }
        }
    }

    if (cand != nullptr && cand->mPrev != nullptr && (cand->mFlags & 0x10000) == 0) {
        uint32_t nx, nz;
        if (cand->mNext != nullptr) {
            nx = PylonOf_2(cand->mNext)->mCellX;
            nz = PylonOf_2(cand->mNext)->mCellZ;
        } else {
            sPylonRecord* p = PylonOf_2(edit->mBraces);
            nx = p->mCellX;
            nz = p->mCellZ;
        }
        sPylonRecord* pp = PylonOf_2(cand->mPrev);
        const uint32_t cx = pp->mCellX;
        const uint32_t cz = pp->mCellZ;
        int32_t ok = 0;
        if (valid != 0) {
            if (IsValidPylonPosition(cx, cz, cand, x, z, nx, nz) != 0) ok = 1;
        }
        valid = ok & 0xff;

        tag_sTrackBrace* far;
        if ((gCoasterInputFlags & 0x100) != 0 && (edit->mBlock3C & 1) != 0) {
            far = cand->mPrev->mPrev;
            if (far == nullptr) far = edit->mLastBrace->mPrev;
        } else {
            far = cand->mPrev->mPrev;
        }
        if (far != nullptr) {
            tag_sTrackBrace* near = cand->mPrev;
            if ((near->mFlags & 0x10000) == 0) {
                sPylonRecord* fp = PylonOf_2(far);
                ok = 0;
                if (valid != 0) {
                    if (IsValidPylonPosition(fp->mCellX, fp->mCellZ, near, cx, cz, x, z) != 0) ok = 1;
                }
                valid = ok & 0xff;
            }
        }
    }
    return valid;
}

// Unit v5190: the editor's pylon placement (TPCoaster.cpp on the Mac): SetPotentialPylonArea 0x10041a58, PlacePylon 0x10041c5c.

// 0x10041a58 (472 bytes): the square of cells around the pylon the editor works on, as far as its control's reach (the root of the
// control's word 0xa0, truncated, plus 1), cut to the height field. The pylon is the dragged brace's, the preview brace's or the first
// brace of the instance without flag 0x2000. Bug kept: the second answer is cut with the x start (gHeightfieldXStart), not the y start.
void SetPotentialPylonArea() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    tag_sTrackBrace* brace;
    if (instance->mBlock3C & 2) {
        if (gCoasterInputFlags & 0x100) {
            brace = gPylonMoveBrace->mNext;
            if (brace->mFlags & 0x10000) brace = gPylonMoveBrace->mPrev;
        } else {
            brace = instance->mPreviewBrace;
        }
    } else if (gCoasterInputFlags & 0x100) {
        brace = gPylonMoveBrace->mNext;
    } else {
        brace = instance->mBraces;
        while (brace != nullptr) {
            if ((brace->mFlags & 0x2000) == 0) break;
            brace = brace->mNext;
        }
    }
    const int32_t x = static_cast<int32_t>(brace->mData->mCellX);
    const int32_t z = static_cast<int32_t>(brace->mData->mCellZ);
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    const sPylonControl* control = (gCoasterInputFlags & 0x40) ? &type->mPylonsB : &type->mPylonsA;
    const float reach = static_cast<float>(static_cast<uint32_t>(control->mFA0));
    const int32_t radius = static_cast<int32_t>(static_cast<float>(std::sqrt(static_cast<double>(reach)))) + 1;
    const int32_t xStart = gHeightfieldXStart;
    int32_t a = x - radius;
    int32_t b = z - radius;
    int32_t c = x + radius;
    int32_t d = z + radius;
    if (a < xStart) a = xStart;
    if (b < xStart) b = xStart;
    if (c >= xStart + gHeightfieldWidth) c = xStart + gHeightfieldWidth - 1;
    if (d >= gHeightfieldYStart + gHeightfieldHeight) d = gHeightfieldYStart + gHeightfieldHeight - 1;
    gCoasterWord689C = static_cast<uint32_t>(a);
    gCoasterWord6898 = static_cast<uint32_t>(b);
    gCoasterWord6894 = static_cast<uint32_t>(c);
    gCoasterWord6890 = static_cast<uint32_t>(d);
}

// 0x10041c5c (988 bytes). A pylon of the cell is looked for in every instance's list (flags 5 skipped); a missing one is made. With the
// editor in state bit 2 and the pylon holding the instance's brace +0x64 the track is rejoined and the call ends with 1. An existing pylon whose
// last brace is the last of the track ends with 0 (flag 0). Otherwise a brace is made, the cost paid (flag 0, no recording) and the loop counter run down.
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, tag_sPylon** outPylon, tag_sTrackBrace** outBrace, uint8_t level, uint32_t extra) {
    uint32_t brace_level = level;
    float loft = 0.0f;
    uint32_t linkLevel = 0;
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    sPylonControl* control = (gCoasterInputFlags & 0x40) ? const_cast<sPylonControl*>(&type->mPylonsB) : const_cast<sPylonControl*>(&type->mPylonsA);
    const uint32_t index = x + z * gHeightfieldHeader->mHField->mF18[0];

    sPylonRecord* pylon = nullptr;
    for (tag_sCoasterInstance* other = gCoasterInstances; other != nullptr; other = other->mNext) {
        sPylonRecord* node = reinterpret_cast<sPylonRecord*>(other->mPylons);
        while (node != nullptr) {
            if ((node->mFlags & 5) == 0 && node->mIndex == index) break;
            node = node->mNext;
        }
        if (node != nullptr) {
            pylon = node;
            break;
        }
    }

    if (pylon == nullptr) {
        pylon = NewPylon(x, z, 0, flag, control, nullptr);
    } else {
        if (instance->mBlock3C & 2) {
            tag_sTrackBrace* wanted = instance->mBrace64;
            for (uint32_t i = 0; i < pylon->mCount; ++i) {
                if (pylon->mBraces[i] == wanted) {
                    RejoinTrack(instance);
                    if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
                    if (outBrace != nullptr) *outBrace = wanted;
                    SetCoasterEditState(0x10);
                    CoasterPanelEndTrack();
                    return 1;
                }
            }
        }
        if (flag == 0 && pylon->mBraces[pylon->mCount - 1]->mNumber == instance->mBraceCount - 1) {
            if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
            return 0;
        }
    }

    if (flag == 0) {
        GetNewBraceLoftInterval(reinterpret_cast<sLinkPylon*>(pylon), control, instance->mPreviewBrace, instance->mBraceB5C,
                                gCoasterWord1790 == 2, 0, &loft);
        CTRACE(5, "Determining link level for new pylon\n");
        DetermineLinkLevel(loft, reinterpret_cast<sLinkPylon*>(pylon), &linkLevel, &loft, 0);
    }
    if (level == 0xff) brace_level = linkLevel & 0xff;

    tag_sTrackBrace* brace =
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, loft, flag != 0, control, 2 - gCoasterWord1790, brace_level & 0xff, extra);
    SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), brace);

    if (flag == 0) {
        tag_sCoasterInstance* edit = gCoasterEditInstance;
        RecalcTrackParams(edit);
        edit->mFlags |= 1;
        edit->mFlags |= 0x1000;
        float cellX = 5.0f + pylon->mX;
        float cellZ = 5.0f + pylon->mZ;
        const float cost = CalculateBraceCost(gCoasterEditInstance, 1, brace, &cellX, &cellZ, nullptr, nullptr);
        CBankAccount* account = gGameWorld->GetBankAccount();
        account->GetBalance();
        if (gActionRecorder.mRecording == 0) account->MakeWithdrawal(static_cast<uint32_t>(FloatToCell(static_cast<double>(cost))));
    }

    if (gCoasterInputFlags & 0x40) {
        gCoasterWord1790 = gCoasterWord1790 - 1;
        if (gCoasterWord1790 == 0) {
            gCoasterInputFlags &= ~0x40u;
            if (flag == 0 && gActionRecorder.mRecording == 0) UIHandleEndLoop();
        } else if (gCoasterWord1790 == 1 && flag == 0 && gActionRecorder.mRecording == 0) {
            UIHandleMidLoop();
        }
    }

    SetPotentialPylonArea();
    if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
    if (outBrace != nullptr) *outBrace = brace;
    return 1;
}
