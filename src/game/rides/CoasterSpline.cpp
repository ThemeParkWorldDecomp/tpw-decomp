// CoasterSpline.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "Debug.h"
#include "keytable_actions.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "tb_matrix.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "ui_main.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// TPConfig.cpp (main app), unit v5275: RoundUp 0x1004da1c (92 bytes) and GetCoasterTrackStats 0x1004d970 (116 bytes).

// 0x1004da1c: the integer part of the value (truncated), plus one when the fraction is not zero. A negative fraction counts too, so
// -1.5 gives -1 (the original's behaviour).
int32_t RoundUp(float value) {
    double whole;
    double fraction = std::modf(static_cast<double>(value), &whole);
    int32_t result = ppc_fctiwz(whole);
    if (0.0f != static_cast<float>(fraction)) result = result + 1;
    return result;
}

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

// A coaster car bent to the track, and a running train's sounds and screams (TPConfig.cpp on the Mac, unit w214): AccumulateBendAngles
// 0x10045ad8, RotateNodeVertexPosns 0x10046620, BendCarToTrack 0x1004688c, HandleScreams 0x10046d78, TickTrainSounds 0x1004702c and
// InterpAngles 0x1004dd5c, with the eight clamps of the bend angles (gBendAngleClamps, 0x1029cd8c).
//
// CoasterCarRideTrackTick (0x1004c4f0, untranslated) calls BendCarToTrack once per car: AccumulateBendAngles reads the sampled track
// around the car's place (include/TPCoaster.h, sTrackSample) and leaves, for every sample from the car's tail to its nose, the yaw and the
// pitch the track turns by there against the car's own direction, and the rail's height; RotateNodeVertexPosns then turns every vertex of
// the car's body node by the angles interpolated at the vertex's z. The sample stride is 0x38 bytes on the Mac and 0x40 on the host (the
// piece pointer), which the array indexing here follows. TickTrainSounds keeps a train's two sound effects at the ride's level and place
// and calls HandleScreams, which starts, moves and stops the riders' screams.
//
// The compare directions follow the original's fcmpo / fcmpu + branch pairs, so a NaN takes the same way; the fused operations are the
// original's fmadds / fmsubs (ppc_math.h), and the project builds with -ffp-contract=off, so no other product-sum is fused. The angles use
// the engine's sine table (gTPGlobals), not the C library's sin and cos; the arc cosine, the square root, fmod and modf are the C library's.

namespace {

const float kPi_2 = 3.1415927f;        // Bits(0x40490fdb)
const float kTwoPi_2 = 6.2831855f;     // Bits(0x40c90fdb)
// InterpAngles adds the float pi and 2 pi widened to double (0x400921fb60000000 at 0x102648b8, 0x401921fb60000000 at 0x102648b0).
const double kPiD = static_cast<double>(kPi_2);
const double kTwoPiD = static_cast<double>(kTwoPi_2);

}  // namespace

// 0x1004dd5c (188 bytes). The angle between a and b by t: the straight interpolation when they are no more than pi apart (a NaN distance
// takes the other way), else the long way round the circle: pi plus the angle of the side that is a (when a >= 0) or b (when a is negative),
// advanced by the weight times the rest of the circle, wrapped by fmod at 2 pi and taken back by pi.
float InterpAngles(float a, float b, float t) {
    const float distance = static_cast<float>(std::fabs(static_cast<double>(a) - static_cast<double>(b)));
    if (distance <= kPi_2) return ppc_fmadds(a, 1.0f - t, t * b);
    const float rest = kTwoPi_2 - distance;
    double sum;
    if (a < 0.0f)
        sum = (kPiD + static_cast<double>(b)) + static_cast<double>((1.0f - t) * rest);
    else
        sum = (kPiD + static_cast<double>(a)) + static_cast<double>(t * rest);
    return static_cast<float>(std::fmod(sum, kTwoPiD)) - kPi_2;
}

// 0x1004de40: the id-th brace back from the block's last brace
tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id) {
    tag_sTrackBrace* brace = params->mLastBrace;
    while (id != 0) {
        brace = brace->mPrev;
        --id;
    }
    return brace;
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

// The coaster track's mesh (main app TPConfig.cpp, unit w213): the spline between two braces, the rings extruded along it, their faces and
// the track's mesh sets. ExtrudeTrackSection 0x1004f628 (1,816 bytes) builds one ring of vertices for each t it is given (a cross section
// of the extrusion table, blended and turned by the orientation the spline gives), skins the faces between the ring before and the new
// one, and calls GetNewTrackMeshSet 0x1004ea30 (816 bytes) for a fresh mesh set when the master set's room runs out. The faces are linked
// into one list per texture (the containers of the face store B, LinkFEStoreToFETextureCont 0x1004e790) and copied into the set's face
// sets when the set is finished (CopyExtrudedFaceData 0x1004e6a0). Also here: SkinCrossSectionFaces 0x1004f410, ExtrudeCrossSectionVertices
// 0x1004ed8c and the sTrackSample overload of InterpolateCoasterSpline 0x1004e318.
//
// Two bases, kept as the listing has them: the type, the track's mesh set, the samples and the vertices' V coordinate are read through
// gCoasterCurrent (re-read before each use), the ring's t, the V counter, the angles and the orientation through gCoasterCurrentData, the
// address of the +0x3c block, which is not cleared after a tick and so may belong to another instance. The mesh sets and the face lists
// live in the game's arena: the Ptr32 words hold arena offsets, so the stores of non-arena addresses stop the program.

// 0x1004e318. The point and the orientation of the track between the controls a and b at t (the stand-in callees write them), then the
// bank: from the samples the controls c and d name when the params' word 0 has bit 8, else blended from the braces' +0xe4 by the
// fraction of the controls' brace numbers. orient is optional: the position alone is wanted then.
void InterpolateCoasterSpline(sTrackSample* samples, sCoasterTrackParams* params, tag_sBezierControl* a, tag_sBezierControl* b,
                              tag_sBezierControl* c, tag_sBezierControl* d, float t, s3DPoint* point, sCoasterOrientation* orient) {
    BezierPosition(a, b, t, point);
    if (orient == nullptr) return;
    s3DPoint tangent;
    BezierTangent(a, b, t, &tangent);
    const float u = 1.0f - t;
    const float bTwist = b->mBrace->mF6C + b->mF0C;
    const float aTwist = a->mBrace->mF6C + a->mF0C;
    const float twist = ppc_fmadds(aTwist, u, bTwist * t);
    GetAlignmentParamsQuick(tangent, twist, orient, nullptr);
    GetBankParams(point, c, d, t, orient);
    if ((*reinterpret_cast<const uint32_t*>(params) & 8) != 0) {
        const uint32_t second = cw_cvt_fp2unsigned(d->mF08);
        const float fromSecond = t * samples[second].mBank;
        const uint32_t first = cw_cvt_fp2unsigned(c->mF08);
        orient->mF0C = orient->mF0C + ppc_fmadds(u, samples[first].mBank, fromSecond);
    } else {
        const float dPlace = static_cast<float>(d->mBrace->mNumber) + d->mF0C;
        const float cPlace = static_cast<float>(c->mBrace->mNumber) + c->mF0C;
        float place = ppc_fmadds(cPlace, u, dPlace * t);
        double whole;
        std::modf(static_cast<double>(cPlace), &whole);
        place = place - static_cast<float>(whole);
        place = (place < 0.0f) ? 0.0f : place;
        place = (1.0f < place) ? 1.0f : place;
        orient->mF0C = orient->mF0C + ppc_fmadds(c->mBrace->mFE4, 1.0f - place, d->mBrace->mFE4 * place);
    }
    orient->mF18 = orient->mF0C;
}
