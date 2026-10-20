// The track editor's brace placement: where the two control arrays of a brace stand (GetActualBraceTrackPositions), the tangents of
// the controls (GetActualBraceTangents) and the lengths of the spans next to a control (GetTangentLengths). TPCoaster.cpp on the Mac.
#include <cmath>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

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
