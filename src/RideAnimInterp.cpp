// The node animation's interpolators of the main app (SimTheme Park 2.04b3): what AnimateNode
// (src/RideAnimTick.cpp) calls to turn an animation's keys, shapes and texture tracks into a
// node's matrix and texture coordinates. Fifteen leaf functions of the rsse.cpp and RideAnim*.cpp
// ranges, all float work:
//   shapes   RABezierInterFromShape 0x100c84ec, RABezierTangentFromShape 0x100c86e8,
//            RALinearInterFromShape 0x100c88e0, RALinearTangentFromShape 0x100c89c8,
//            RACatmulInterFromShape 0x100c8a8c, RAInterpPosnAtFrame 0x100c8ca8
//   matrices SetLMatrixOrientation 0x100c1eac, SetLMatrixScaleNoTrans 0x100c1ab8,
//            SetLMatrixXZOrientation 0x100c2ee8, CombineMatricesNoWDest2nd (s3DAxis) 0x100c37a4
//   rotation RACalcOrientationBetweenControlPoints 0x100c8054, RASphericalLinearInterpolate
//            0x100c7db0, RAQuaternionToLAxis 0x100c7ca0
//   textures RAAnimateTextureCoords 0x100c82d0, RAGetUVFrameData 0x100c81a0
// The CodeWarrior compiler fused many of the products and sums; each helper below says which
// instructions it stands for, and the fused ones go through include/ppc_math.h.
#include <cmath>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// divwu, mullw and subf of the listing: a % n. The PowerPC leaves the quotient of a division by
// zero undefined; Unicorn answers 0 and so does this (the remainder is then a).
uint32_t UnsignedRemainder(uint32_t a, uint32_t n) {
    const uint32_t quotient = n != 0 ? a / n : 0;
    return a - quotient * n;
}

// The whole part of a shape's fraction and what is left of it: fctiwz (saturating), the whole
// number as a single, and the single-precision difference (the unfused fsubs of the listing).
struct SplitFraction {
    int32_t whole;
    float rest;
};

SplitFraction Split(float fraction) {
    const int32_t whole = ppc_fctiwz(static_cast<double>(fraction));
    return {whole, fraction - static_cast<float>(whole)};
}

// The points a shape function works on: the array it is given, else the shape's own.
const s3DPoint* PointsOf(const tag_sShape* shape, const s3DPoint* given) {
    return given != nullptr ? given : shape->mPoints.get();
}

// A point of an array by an index that the Mac works out in 32 bits (a wrapped negative index
// reaches back before the array).
const s3DPoint& PointAt(const s3DPoint* points, uint32_t index) { return points[static_cast<int32_t>(index)]; }

// The four points around `index` of a ring of `count` points (RABezier*, RACatmul*): the point
// before it, itself, and the two after it, each as (index + count + offset) % count. `base` is
// index + count, so that the point before cannot go below 0.
struct Ring {
    const s3DPoint* before;
    const s3DPoint* at;
    const s3DPoint* next;
    const s3DPoint* after;
};

Ring RingAround(const s3DPoint* points, uint32_t base, uint32_t count) {
    return {&PointAt(points, UnsignedRemainder(base - 1, count)), &PointAt(points, UnsignedRemainder(base, count)),
            &PointAt(points, UnsignedRemainder(base + 1, count)), &PointAt(points, UnsignedRemainder(base + 2, count))};
}

// One coordinate of RABezierInterFromShape: the cubic bezier segment that starts at b, with the
// control points a and c and the end d. They are the points at index, index - 1, index + 1 and
// index + 2 of the ring (AnimateNode passes the index 3 * segment + 1, so b is the segment's own
// first point). The listing's fused sequence, which builds the result on b.
float BezierCoordinate(float a, float b, float c, float d, float t) {
    const float threeA = 3.0f * a;
    float cubic = -6.0f * a;
    float quadratic = ppc_fmadds(-1.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, b, cubic);
    const float linear = ppc_fmadds(-3.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, c, cubic);
    quadratic = ppc_fmadds(-3.0f, c, quadratic);
    cubic = t * cubic;
    quadratic = d + quadratic;
    cubic = t * cubic;
    quadratic = t * quadratic;
    quadratic = t * quadratic;
    cubic = ppc_fmadds(t, quadratic, cubic);
    cubic = ppc_fmadds(t, linear, cubic);
    return b + cubic;
}

// One coordinate of RABezierTangentFromShape (same names).
float BezierSlope(float a, float b, float c, float d, float t) {
    const float threeA = 3.0f * a;
    float cubic = -6.0f * a;
    float quadratic = ppc_fmadds(-1.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, b, cubic);
    const float linear = ppc_fmadds(-3.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, c, cubic);
    quadratic = ppc_fmadds(-3.0f, c, quadratic);
    cubic = 2.0f * cubic;
    quadratic = d + quadratic;
    cubic = t * cubic;
    quadratic = 3.0f * quadratic;
    quadratic = t * quadratic;
    cubic = ppc_fmadds(t, quadratic, cubic);
    return linear + cubic;
}

// One coordinate of RACatmulInterFromShape: p0 to p3 are the points before, at, after and two
// after the index.
float CatmullCoordinate(float p0, float p1, float p2, float p3, float t) {
    const float negP0 = -1.0f * p0;
    float even = -5.0f * p1;
    float odd = ppc_fmadds(3.0f, p1, negP0);
    even = ppc_fmadds(2.0f, p0, even);
    odd = ppc_fmadds(-3.0f, p2, odd);
    even = ppc_fmadds(4.0f, p2, even);
    const float diff = negP0 + p2;
    odd = p3 + odd;
    even = even - p3;
    odd = t * odd;
    even = t * even;
    odd = t * odd;
    even = t * even;
    even = ppc_fmadds(t, odd, even);
    even = ppc_fmadds(t, diff, even);
    even = ppc_fmadds(2.0f, p1, even);
    return 0.5f * even;
}

// The listing's frsp(fmsub(a, b, c * d)): the product of two floats is exact in double, the
// fused difference is rounded to double and then to single. One component of a cross product.
float CrossTerm(float a, float b, float c, float d) {
    const double product = static_cast<double>(c) * static_cast<double>(d);
    return static_cast<float>(ppc_fmsub(a, b, product));
}

// One row of SetLMatrixScaleNoTrans: the row made as long as `scale`. Its length is worked out
// in double precision: m0 * m0 fused with the SINGLE-rounded square of m1, then the single-rounded
// square of m2 added, then sqrt and frsp; the factor is the single division scale / length.
// `scale` is read after the length, as the original does.
void ScaleRow(float* row, const float& scale) {
    const float square1 = row[1] * row[1];
    const float square2 = row[2] * row[2];
    double sum = ppc_fmadd(row[0], row[0], square1);
    sum = static_cast<double>(square2) + sum;
    const float length = static_cast<float>(std::sqrt(sum));
    const float factor = scale / length;
    row[0] = row[0] * factor;
    row[1] = row[1] * factor;
    row[2] = row[2] * factor;
}

// The sine table of TPGlobals at the angle whose table index (angle * scale, truncated) is
// `scaled`; the index is and-ed with the mask, so a negative angle wraps.
float SineAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

constexpr float kHalfPi = 1.5707964f;   // 0x3fc90fdb

}  // namespace

// 0x100c8ca8, 356 bytes. See rsse.h. The track's values are percentages that wrap round at 100: when
// the two neighbours are more than 50 apart the lower gets 100 added, and the blend is taken modulo
// 100. A frame equal to the count (its whole part) stands on the last value with a fraction one more.
// Without `wrap` the last value is its own neighbour; with it the neighbour of the last is the first.
// (A zero count with `wrap` divides by zero: undefined on the PowerPC.)
void RAInterpPosnAtFrame(float* values, uint32_t count, float frame, float* out, int32_t wrap) {
    double wholeFrame;
    float fraction = static_cast<float>(std::modf(static_cast<double>(frame), &wholeFrame));
    uint32_t index = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(static_cast<float>(wholeFrame))));
    if (index == count) {
        index = index - 1;
        fraction = fraction + 1.0f;
    }
    float low;
    float high;
    if (wrap != 0) {
        low = values[static_cast<int32_t>(index)];
        high = values[UnsignedRemainder(index + 1, count)];
    } else {
        low = values[static_cast<int32_t>(index)];
        high = index < count - 1 ? values[static_cast<int32_t>(index) + 1] : low;
    }
    const float apart = static_cast<float>(std::fabs(static_cast<double>(low) - static_cast<double>(high)));
    if (apart > 50.0f) {
        if (low < high) low = low + 100.0f;
        else high = high + 100.0f;
        *out = ppc_fmadds(1.0f - fraction, low, fraction * high);
        *out = static_cast<float>(std::fmod(static_cast<double>(*out), 100.0));
    } else {
        *out = ppc_fmadds(1.0f - fraction, low, fraction * high);
    }
}

// 0x100c84ec, 428 bytes. The cubic bezier point of the shape (three points a segment, the points at
// 3 * whole + index - 1 to + 2 of a ring of TPGlobals' word 0x4020 points) at the fractional part of
// `fraction`.
void RABezierInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) * 3 + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = BezierCoordinate(ring.at->x, ring.before->x, ring.next->x, ring.after->x, t);
    out->y = BezierCoordinate(ring.at->y, ring.before->y, ring.next->y, ring.after->y, t);
    out->z = BezierCoordinate(ring.at->z, ring.before->z, ring.next->z, ring.after->z, t);
}

// 0x100c86e8, 420 bytes. The bezier's direction at the same place (the derivative of the above).
void RABezierTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) * 3 + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = BezierSlope(ring.at->x, ring.before->x, ring.next->x, ring.after->x, t);
    out->y = BezierSlope(ring.at->y, ring.before->y, ring.next->y, ring.after->y, t);
    out->z = BezierSlope(ring.at->z, ring.before->z, ring.next->z, ring.after->z, t);
}

// 0x100c88e0, 152 bytes. The point between points (index + whole) and the next, at the fractional
// part; each coordinate is (1 - t) * from fused with t * to. No wrapping: an index past the shape's
// last point reads whatever follows.
void RALinearInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const s3DPoint* points = PointsOf(shape, a);
    const SplitFraction split = Split(fraction);
    const uint32_t place = index + static_cast<uint32_t>(split.whole);
    const s3DPoint& from = PointAt(points, place);
    const s3DPoint& to = PointAt(points, place + 1);
    const float t = split.rest;
    const float rest = 1.0f - t;
    out->x = ppc_fmadds(rest, from.x, t * to.x);
    out->y = ppc_fmadds(rest, from.y, t * to.y);
    out->z = ppc_fmadds(rest, from.z, t * to.z);
}

// 0x100c89c8, 112 bytes. The step from point (index + whole) to the next: to - from. The fraction
// only picks the segment. Quirk: the original compares `a` with null but no branch follows, so it
// always takes the shape's own points and `a` is not used (AnimateNode always passes null).
void RALinearTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const s3DPoint* points = shape->mPoints.get();
    const uint32_t place = index + static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fraction)));
    const s3DPoint& from = PointAt(points, place);
    const s3DPoint& to = PointAt(points, place + 1);
    out->x = to.x - from.x;
    out->y = to.y - from.y;
    out->z = to.z - from.z;
}

// 0x100c8a8c, 460 bytes. The Catmull-Rom point between points (index + whole) and the next on a ring
// of TPGlobals' word 0x4020 points; the four points are the one before, the two and the one after.
void RACatmulInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = CatmullCoordinate(ring.before->x, ring.at->x, ring.next->x, ring.after->x, t);
    out->y = CatmullCoordinate(ring.before->y, ring.at->y, ring.next->y, ring.after->y, t);
    out->z = CatmullCoordinate(ring.before->z, ring.at->z, ring.next->z, ring.after->z, t);
}

// 0x100c1eac, 100 bytes. A null axis leaves its row alone.
void SetLMatrixOrientation(sMatrix* matrix, s3DPoint* x, s3DPoint* y, s3DPoint* z) {
    s3DPoint* const axes[3] = {x, y, z};
    for (int32_t row = 0; row < 3; ++row) {
        if (axes[row] == nullptr) continue;
        matrix->m[row][0] = axes[row]->x;
        matrix->m[row][1] = axes[row]->y;
        matrix->m[row][2] = axes[row]->z;
    }
}

// 0x100c1ab8, 328 bytes. Row 0 gets the length x, row 1 y and row 2 z; the translation is not touched.
void SetLMatrixScaleNoTrans(sMatrix* matrix, float& x, float& y, float& z) {
    ScaleRow(matrix->m[0], x);
    ScaleRow(matrix->m[1], y);
    ScaleRow(matrix->m[2], z);
}

// 0x100c2ee8, 384 bytes. The matrix's old row 1 is the "up" the new axes are made from: row 0 is
// up x direction made a unit vector (length in double precision, the squares of y and z rounded to
// single first; zero when the two are parallel, then NaN), row 1 is direction x row 0, row 2 the
// direction as given (not made a unit vector). The original tests the direction for null at the end
// although it has used it from the start, so that test cannot fail and is not kept.
void SetLMatrixXZOrientation(sMatrix* matrix, s3DPoint* direction) {
    const float upX = matrix->m[1][0];
    const float upY = matrix->m[1][1];
    const float upZ = matrix->m[1][2];
    const float dirX = direction->x;
    const float dirY = direction->y;
    const float dirZ = direction->z;
    float sideX = CrossTerm(upY, dirZ, upZ, dirY);
    float sideY = CrossTerm(upZ, dirX, upX, dirZ);
    float sideZ = CrossTerm(upX, dirY, upY, dirX);
    const float squareY = sideY * sideY;
    const float squareZ = sideZ * sideZ;
    double sum = static_cast<double>(sideX) * static_cast<double>(sideX) + static_cast<double>(squareY);
    sum = static_cast<double>(squareZ) + sum;
    const float length = static_cast<float>(std::sqrt(sum));
    sideX = sideX / length;
    sideY = sideY / length;
    sideZ = sideZ / length;
    const float outX = CrossTerm(dirY, sideZ, dirZ, sideY);
    const float outY = CrossTerm(dirZ, sideX, dirX, sideZ);
    const float outZ = CrossTerm(dirX, sideY, dirY, sideX);
    matrix->m[0][0] = sideX;
    matrix->m[0][1] = sideY;
    matrix->m[0][2] = sideZ;
    matrix->m[1][0] = outX;
    matrix->m[1][1] = outY;
    matrix->m[1][2] = outZ;
    matrix->m[2][0] = dirX;
    matrix->m[2][1] = dirY;
    matrix->m[2][2] = dirZ;
}

// 0x100c37a4, 292 bytes. For each of the first three columns and rows 0 to 2 (no translation row
// and no fourth column): source row r (the r-th axis) times the destination's old column, as
// y * old[1] fused with x * old[0] and then z * old[2]. The old column is read before it is written.
void CombineMatricesNoWDest2nd(s3DAxis* source, sMatrix* destination) {
    const s3DPoint* const rows[3] = {&source->mX, &source->mY, &source->mZ};
    for (int32_t column = 0; column < 3; ++column) {
        const float old0 = destination->m[0][column];
        const float old1 = destination->m[1][column];
        const float old2 = destination->m[2][column];
        for (int32_t row = 0; row < 3; ++row) {
            float sum = rows[row]->y * old1;
            sum = ppc_fmadds(rows[row]->x, old0, sum);
            sum = ppc_fmadds(rows[row]->z, old2, sum);
            destination->m[row][column] = sum;
        }
    }
}

// 0x100c7ca0, 208 bytes. The rotation matrix of the quaternion (x, y, z, w), scaled by 2 / |q|^2, whose
// squared length is summed as w*w + (z*z + (x*x + y*y)). Nothing is fused; row 0 is
// (1 - (yy + zz), xy + wz, xz - wy), row 1 (xy - wz, 1 - (xx + zz), yz + wx), row 2
// (xz + wy, yz - wx, 1 - (xx + yy)), each product of two coordinates made of one scaled by 2 / |q|^2.
void RAQuaternionToLAxis(sQuaternion* quaternion, s3DAxis* out) {
    const float x = quaternion->x;
    const float y = quaternion->y;
    const float z = quaternion->z;
    const float w = quaternion->w;
    const float norm = (w * w) + ((z * z) + ((x * x) + (y * y)));
    const float scale = 2.0f / norm;
    const float scaledZ = z * scale;
    const float scaledY = y * scale;
    const float scaledX = x * scale;
    const float zz = z * scaledZ;
    const float yy = y * scaledY;
    const float wz = w * scaledZ;
    const float xy = x * scaledY;
    const float wy = w * scaledY;
    const float xz = x * scaledZ;
    const float xx = x * scaledX;
    const float wx = w * scaledX;
    const float yz = y * scaledZ;
    out->mX.x = 1.0f - (yy + zz);
    out->mX.y = xy + wz;
    out->mX.z = xz - wy;
    out->mY.x = xy - wz;
    out->mY.y = 1.0f - (xx + zz);
    out->mY.z = yz + wx;
    out->mZ.x = xz + wy;
    out->mZ.y = yz - wx;
    out->mZ.z = 1.0f - (xx + yy);
}

// 0x100c7db0, 580 bytes. See rsse.h. dot = a . b (fused). With 1 + dot not above 0.001 (or NaN) the
// quaternions are nearly opposite: out = (-a.y, a.x, -a.w, a.z) (a quarter turn), blended with a by the
// sines of t and 1 - t times a quarter turn, in x, y and z only. With 1 - dot above 0.001 they are
// apart: the weights are sin((1 - t) * theta) / sin(theta) for a and sin(t * theta) / sin(theta) for
// b, theta = acos(dot) as a single, the sines from the table. Else they are close: weights 1 - t and t.
void RASphericalLinearInterpolate(sQuaternion* a, sQuaternion* b, float t, sQuaternion* out) {
    float dot = a->y * b->y;
    dot = ppc_fmadds(a->x, b->x, dot);
    dot = ppc_fmadds(a->z, b->z, dot);
    dot = ppc_fmadds(a->w, b->w, dot);
    constexpr float kEpsilon = 0.001f;   // 0x3a83126f
    if (!(1.0f + dot > kEpsilon)) {
        out->x = ppc_fneg(a->y);
        out->y = a->x;
        out->z = ppc_fneg(a->w);
        out->w = a->z;
        const float rest = 1.0f - t;
        const float scale = gTPGlobals.mSinTableScale;
        const float weightB = SineAt((kHalfPi * t) * scale);
        const float weightA = SineAt((kHalfPi * rest) * scale);
        // (out->w is not blended.)
        out->x = ppc_fmadds(weightA, a->x, weightB * out->x);
        out->y = ppc_fmadds(weightA, a->y, weightB * out->y);
        out->z = ppc_fmadds(weightA, a->z, weightB * out->z);
        return;
    }
    float weightA;
    float weightB;
    if (1.0f - dot > kEpsilon) {
        const float theta = static_cast<float>(std::acos(static_cast<double>(dot)));
        const float scale = gTPGlobals.mSinTableScale;
        const float rest = 1.0f - t;
        const float scaledT = scale * (t * theta);
        const float scaledRest = scale * (rest * theta);
        const float scaledTheta = theta * scale;
        const float sinTheta = SineAt(scaledTheta);
        weightA = SineAt(scaledRest) / sinTheta;
        weightB = SineAt(scaledT) / sinTheta;
    } else {
        weightA = 1.0f - t;
        weightB = t;
    }
    out->x = ppc_fmadds(weightA, a->x, weightB * b->x);
    out->y = ppc_fmadds(weightA, a->y, weightB * b->y);
    out->z = ppc_fmadds(weightA, a->z, weightB * b->z);
    out->w = ppc_fmadds(weightA, a->w, weightB * b->w);
}

// 0x100c8054, 248 bytes. See rsse.h. The next key is the one after `index`, or `index` again when it
// is not below TPGlobals' word 0x401c less one (compared as unsigned: with 0 keys every index has a
// next). The plain blend is (1 - t) * from fused with t * to, in all four numbers.
void RACalcOrientationBetweenControlPoints(sQuatKey* keys, uint32_t index, float fraction, s3DAxis* out) {
    const uint32_t next = index < gTPGlobals.mF401C - 1 ? index + 1 : index;
    sQuaternion* from = reinterpret_cast<sQuaternion*>(keys[static_cast<int32_t>(index)].mF04);
    sQuaternion* to = reinterpret_cast<sQuaternion*>(keys[static_cast<int32_t>(next)].mF04);
    sQuaternion blended;
    if ((gTPGlobals.mEngineFlags & 2) != 0) {
        const float rest = 1.0f - fraction;
        blended.x = ppc_fmadds(from->x, rest, to->x * fraction);
        blended.y = ppc_fmadds(from->y, rest, to->y * fraction);
        blended.z = ppc_fmadds(from->z, rest, to->z * fraction);
        blended.w = ppc_fmadds(from->w, rest, to->w * fraction);
    } else {
        RASphericalLinearInterpolate(from, to, fraction, &blended);
    }
    RAQuaternionToLAxis(&blended, out);
}

// 0x100c81a0, 232 bytes. The frame is the control's mF20 as an unsigned count. The first key whose
// number is above it ends the scan: *index is the key before (for the first key that is -1, and the
// number before the array is read), the fraction is (frame - before) / (after - before) with the
// frame as the float mF20, not the count. With no such key *index is count - 2 and *fraction 1.0.
void RAGetUVFrameData(sAnimTimeControl*& control, uint16_t*& frames, uint32_t count, uint32_t& index, float& fraction) {
    const uint32_t frame = cw_cvt_fp2unsigned(static_cast<double>(control->mF20));
    const uint16_t* key = frames;
    for (uint32_t i = 0; i < count; ++i, ++key) {
        if (frame >= *key) continue;
        index = i - 1;
        const float before = static_cast<float>(frames[static_cast<int32_t>(index)]);
        const float after = static_cast<float>(*key);
        fraction = (control->mF20 - before) / (after - before);
        return;
    }
    index = count - 2;
    fraction = 1.0f;
}

// 0x100c82d0, 460 bytes. See rsse.h. Track k of the node animation's texture coordinates has its
// frame numbers from mF0C and its (u, v) pairs from mF10, both from the track's first place; the
// pair is the blend of the one at the key RAGetUVFrameData finds and the next, (1 - t) * from fused
// with t * to. The node's sets hold four u and then four v each.
void RAAnimateTextureCoords(sAnimTimeControl* control, tag_sNodeAnim* nodeAnim, int32_t additive) {
    tag_sNode* node = nodeAnim->mNode.get();
    sUVAnim* animation = nodeAnim->mUVAnim.get();
    for (uint32_t k = 0; k < animation->mTrackCount; ++k) {
        const sUVTrackControl& track = animation->mTrack.get()[k];
        uint16_t* frames = animation->mF0C.get() + track.mF00;
        const float* pairs = animation->mF10.get() + 2 * static_cast<uint32_t>(track.mF00);
        uint32_t index;
        float fraction;
        RAGetUVFrameData(control, frames, track.mF02, index, fraction);
        // (The index is -1 or -2 for a frame before the track's first key; the Mac works the address
        // out in 32 bits, which a signed index reproduces.)
        const float* from = pairs + 2 * static_cast<int32_t>(index);
        const float* to = from + 2;
        const float rest = 1.0f - fraction;
        const float u = ppc_fmadds(rest, from[0], fraction * to[0]);
        const float v = ppc_fmadds(rest, from[1], fraction * to[1]);
        float* set = node->mUVs.get()[k >> 2].mF00;
        float& slotU = set[k & 3];
        float& slotV = set[4 + (k & 3)];
        if (additive != 0) {
            slotU = slotU + u;
            slotV = slotV + v;
        } else {
            slotU = u;
            slotV = v;
        }
    }
}
