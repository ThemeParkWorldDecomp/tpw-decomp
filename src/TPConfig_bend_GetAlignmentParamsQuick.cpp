#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "Advisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

namespace {

const float kHalfPi = 1.5707963705062866f;   // 0x102648a8
const float kTwoPi = 6.2831854820251465f;    // 0x102648c0
const float kPi = 3.1415927410125732f;       // 0x102648c8

// The sine and cosine of -angle from the table: index = mask & fctiwz(value * scale).
inline void SinCosNeg(float angle, float& sinNeg, float& cosNeg) {
    const uint32_t mask = gTPGlobals.mSinTableMask;
    const float scale = gTPGlobals.mSinTableScale;
    const float neg = -angle;
    const float a = neg * scale;
    const float b = (kHalfPi + neg) * scale;
    const uint32_t ia = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(a)));
    const uint32_t ib = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(b)));
    sinNeg = gTPGlobals.mSinTable[mask & ia];
    cosNeg = gTPGlobals.mSinTable[mask & ib];
}

// One Bezier component: position (tangent == false) or tangent.
inline float BezierComponent(const float* a, const float* b, float t, int i, bool tangent) {
    const float d = a[0x4c / 4 + i];
    const float p0 = a[0x40 / 4 + i];
    const float q = b[0x34 / 4 + i];
    const float p1 = b[0x40 / 4 + i];
    const float d3 = 3.0f * d;
    const float d6 = -6.0f * d;
    float s5 = ppc_fmadds(-1.0f, p0, d3);
    float s4 = ppc_fmadds(3.0f, p0, d6);
    s5 = ppc_fmadds(-3.0f, q, s5);
    s4 = ppc_fmadds(3.0f, q, s4);
    const float s7 = ppc_fmadds(-3.0f, p0, d3);
    s5 = p1 + s5;
    if (!tangent) {
        s4 = t * s4;
        s5 = t * s5;
        s4 = t * s4;
        s5 = t * s5;
        s4 = ppc_fmadds(t, s5, s4);
        s4 = ppc_fmadds(t, s7, s4);
        return p0 + s4;
    }
    s4 = 2.0f * s4;
    s5 = 3.0f * s5;
    s4 = t * s4;
    s5 = t * s5;
    s4 = ppc_fmadds(t, s5, s4);
    return s7 + s4;
}

inline const float* Words(const tag_sBezierControl* c) { return reinterpret_cast<const float*>(c); }

}  // namespace

// 0x1004da98 (268 bytes)
void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point) {
    point->x = BezierComponent(Words(a), Words(b), t, 0, false);
    point->y = BezierComponent(Words(a), Words(b), t, 1, false);
    point->z = BezierComponent(Words(a), Words(b), t, 2, false);
}

// 0x1004dc00 (260 bytes)
void BezierTangent(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* tangent) {
    tangent->x = BezierComponent(Words(a), Words(b), t, 0, true);
    tangent->y = BezierComponent(Words(a), Words(b), t, 1, true);
    tangent->z = BezierComponent(Words(a), Words(b), t, 2, true);
}

// 0x1004de98 (356 bytes). The original also writes a word at +0x1c of the orientation (a copy of the first angle), past the
// 0x1c bytes the header declares; kept.
void GetAlignmentParamsQuick(s3DPoint tangent, float twist, sCoasterOrientation* orient, float* out) {
    (void)twist;
    float* words = reinterpret_cast<float*>(orient);
    s3DPoint p = tangent;
    if (out != nullptr)
        orient->mF04 = *out;
    else
        orient->mF04 = GetVectorRotationAroundYAxisFromZ(p);
    float sinNeg, cosNeg;
    SinCosNeg(orient->mF04, sinNeg, cosNeg);
    const float x = p.x;
    const float z = p.z;
    p.x = ppc_fmadds(x, cosNeg, z * sinNeg);
    p.z = ppc_fmadds(-x, sinNeg, z * cosNeg);
    orient->mF08 = GetVectorRotationAroundXAxisFromZ(p);
    words[7] = orient->mF04;
    uint32_t flags;
    std::memcpy(&flags, &orient->mF00, 4);
    if (flags & 1) {
        orient->mF04 = orient->mF04 + kPi;
        orient->mF08 = kPi - orient->mF08;
        float v = orient->mF04;
        if (v > kPi) orient->mF04 = v - kTwoPi;
        v = orient->mF08;
        if (v > kPi) orient->mF08 = v - kTwoPi;
    }
    orient->mF10 = orient->mF04;
    orient->mF14 = orient->mF08;
}

// 0x1004e04c (372 bytes)
void GetBankParams(s3DPoint* point, tag_sBezierControl* c, tag_sBezierControl* d, float t, sCoasterOrientation* orient) {
    s3DPoint p;
    BezierPosition(c, d, t, &p);
    p.x = p.x - point->x;
    p.y = p.y - point->y;
    p.z = p.z - point->z;
    float sinNeg, cosNeg;
    SinCosNeg(orient->mF04, sinNeg, cosNeg);
    const float x = p.x;
    const float z = p.z;
    p.x = ppc_fmadds(x, cosNeg, z * sinNeg);
    p.z = ppc_fmadds(-x, sinNeg, z * cosNeg);
    SinCosNeg(orient->mF08, sinNeg, cosNeg);
    const float y = p.y;
    const float z2 = p.z;
    p.y = ppc_fmsubs(y, cosNeg, z2 * sinNeg);
    p.z = ppc_fmadds(y, sinNeg, z2 * cosNeg);
    orient->mF0C = GetVectorRotationAroundZAxisFromY(p);
}

// 0x1004e230 (128 bytes)
void GetAlignmentParamsSlow(sCoasterTrackParams* params, s3DPoint* point, float f, sCoasterOrientation* orientation, int32_t a, float* out) {
    SetInitialOrientationParams(orientation, params, a);
    GetAlignmentParamsQuick(*point, f, orientation, out);
}
