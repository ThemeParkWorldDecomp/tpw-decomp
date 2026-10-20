#include <cmath>
#include <cstdio>
#include <cstring>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

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
