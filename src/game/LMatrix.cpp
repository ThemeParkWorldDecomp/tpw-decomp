// LMatrix.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_DrawObject.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// The main application's matrix helpers. Its back substitution uses separate multiply
// and subtract instructions; its decomposition uses PPC fused negative subtracts.

void SetMatrixIdentity(sMatrix* matrix) {
    if (matrix != nullptr) std::memset(matrix, 0, sizeof *matrix);
    for (int i = 0; i < 4; ++i) matrix->m[i][i] = 1.0f;
}

// rsse.cpp (main app of SimTheme Park 2.04b3): the matrix and vector helpers between 0x100c0eac and 0x100c3198 that had no translation
// (unit w170): identity of the 3 x 3 part and row 3, the angles of a matrix or a vector (atan2 of doubles rounded to single), the
// four-row axis rotations, a scale by three floats, a vector through the 3 x 3 part, the 3 x 3 product that writes its second
// argument, the distance of two points, and FloatSortByArray (a recursive merge sort of object pointers by a float member).
// The first fourteen came in from a translation made on a private test bench (puddle-challenges/q001) and were checked line by line
// against the listings; every fused call below is an fmadds, fmsubs or fmadd of the listing, every product that is not fused is an fmuls.

// 0x100c0eac, 64 bytes: the diagonal (m33, m22, m11, m00) is 1.0f and the rest of the 3 x 4 block is 0.0f; m30, m31, m32 (row 3's
// translation) are NOT written.
void SetLMatrixIdentityNoTrans(sMatrix* matrix) {
    matrix->m[3][3] = 1.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[0][0] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][1] = 0.0f;
}

// 0x100c0f24, 104 bytes: the turn about y. The listing compares m[2][1] with 0.9f first (bgt: greater takes the first form), then with
// -0.9f (bge branches on a clear LT bit, so a NaN, which sets only the unordered bit, takes the second form); the first form is
// atan2(-m[0][2], m[0][0]), the second atan2(m[2][0], m[2][2]).
float GetLMatrixRotationAroundYAxis(sMatrix* matrix) {
    const float v = matrix->m[2][1];
    double r;
    if (v > 0.9f || v < -0.9f) {
        r = std::atan2(-static_cast<double>(matrix->m[0][2]), static_cast<double>(matrix->m[0][0]));
    } else {
        r = std::atan2(static_cast<double>(matrix->m[2][0]), static_cast<double>(matrix->m[2][2]));
    }
    return static_cast<float>(r);
}

float GetLMatrixRotationAroundZAxisYOnly(sMatrix* matrix) {
    return static_cast<float>(std::atan2(-static_cast<double>(matrix->m[1][0]), static_cast<double>(matrix->m[1][1])));
}

// 0x100c1040, 52 bytes: atan2(-y, z) of the point, in double precision rounded to single.
float GetVectorRotationAroundXAxisFromZ(s3DPoint& vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector.y), static_cast<double>(vector.z)));
}

// 0x100c11a4, 52 bytes: the pointer form of 0x100c1040.
float GetVectorRotationAroundXAxisFromZ(s3DPoint* vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector->y), static_cast<double>(vector->z)));
}

// 0x100c10b8, 48 bytes: atan2(x, z).
float GetVectorRotationAroundYAxisFromZ(s3DPoint& vector) {
    return static_cast<float>(std::atan2(static_cast<double>(vector.x), static_cast<double>(vector.z)));
}

// 0x100c121c, 48 bytes: the pointer form of 0x100c10b8 (the one the original's callers use).
float GetVectorRotationAroundYAxisFromZ(s3DPoint* vector) {
    return static_cast<float>(std::atan2(static_cast<double>(vector->x), static_cast<double>(vector->z)));
}

// 0x100c112c, 52 bytes: atan2(-x, y).
float GetVectorRotationAroundZAxisFromY(s3DPoint& vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector.x), static_cast<double>(vector.y)));
}

// 0x100c1290, 52 bytes: the pointer form of 0x100c112c.
float GetVectorRotationAroundZAxisFromY(s3DPoint* vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector->x), static_cast<double>(vector->y)));
}

// 0x100c1308, 132 bytes: all FOUR rows (the translation too, unlike XRotateLMatrixNoTrans 0x100c13bc) turned about x: columns 1 and 2
// become a*c - b*s and a*s + b*c, the product of b rounded to single, the other fused.
void XRotateLMatrix(sMatrix* matrix, float c, float s) {
    for (int r = 0; r < 4; r++) {
        const float a = matrix->m[r][1];
        const float b = matrix->m[r][2];
        matrix->m[r][1] = ppc_fmsubs(a, c, b * s);
        matrix->m[r][2] = ppc_fmadds(a, s, b * c);
    }
}

// rsse.cpp (main app of SimTheme Park 2.04b3): turning a matrix's 3 x 3 part about the axes (X/Y/ZRotateLMatrixNoTrans
// 0x100c13bc, 0x100c1698, 0x100c15f0, RotateLMatrixXYZNoTrans 0x100c174c, RotateLMatrixXYZNoTransHi 0x100c1944).
// Each row of the matrix is turned on its own (row 3, the translation, is not), as the row vector times the rotation;
// every product is first rounded to single and the sum or difference is fused with the product before it.

namespace {

// The orders the eRotateParams values name (the table at 0x1029ebdc): the characters that are not X, Y or Z do nothing.
const char* const kRotateOrders[16] = {"FUK", "X",  "Y",  "Z",   "XY",  "YX",  "YZ",  "ZY",
                                       "XZ",  "ZX", "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};

// The constant at 0x10264e38: pi / 2 as a single.
constexpr float kHalfPi = 1.57079637050628662109375f;

// Turns the matrix about the axis named by the character (the X, Y or Z of an order's string) by the angle whose
// cosine and sine these are.
void RotateAbout(sMatrix* matrix, char axis, float c, float s) {
    if (axis == 'X') XRotateLMatrixNoTrans(matrix, c, s);
    else if (axis == 'Y') YRotateLMatrixNoTrans(matrix, c, s);
    else ZRotateLMatrixNoTrans(matrix, c, s);
}

// Whether the character is one of the three axes (the original compares it as a signed char).
bool IsAxis(char character) { return character == 'X' || character == 'Y' || character == 'Z'; }

}  // namespace

void XRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) {
    for (int row = 0; row < 3; ++row) {
        float* r = matrix->m[row];
        const float y = r[1];
        const float z = r[2];
        const float zs = z * s;
        const float zc = z * c;
        r[1] = ppc_fmsubs(y, c, zs);
        r[2] = ppc_fmadds(y, s, zc);
    }
}

// 0x100c1458, 164 bytes: four rows about y: columns 0 and 2 become a*c + b*s and (-a)*s + b*c.
void YRotateLMatrix(sMatrix* matrix, float c, float s) {
    for (int r = 0; r < 4; r++) {
        const float a = matrix->m[r][0];
        const float b = matrix->m[r][2];
        matrix->m[r][0] = ppc_fmadds(a, c, b * s);
        matrix->m[r][2] = ppc_fmadds(-a, s, b * c);
    }
}

// 0x100c152c, 148 bytes: four rows about z: columns 0 and 1 become a*c - b*s and a*s + b*c.
void ZRotateLMatrix(sMatrix* matrix, float c, float s) {
    for (int r = 0; r < 4; r++) {
        const float a = matrix->m[r][0];
        const float b = matrix->m[r][1];
        matrix->m[r][0] = ppc_fmsubs(a, c, b * s);
        matrix->m[r][1] = ppc_fmadds(a, s, b * c);
    }
}

void ZRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) {
    for (int row = 0; row < 3; ++row) {
        float* r = matrix->m[row];
        const float x = r[0];
        const float ys = r[1] * s;
        r[0] = ppc_fmsubs(x, c, ys);
        const float yc = r[1] * c;
        r[1] = ppc_fmadds(x, s, yc);
    }
}

void YRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) {
    for (int row = 0; row < 3; ++row) {
        float* r = matrix->m[row];
        const float x = r[0];
        const float negatedX = -x;
        const float zs = r[2] * s;
        r[0] = ppc_fmadds(x, c, zs);
        const float zc = r[2] * c;
        r[2] = ppc_fmadds(negatedX, s, zc);
    }
}

void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle) {
    for (const char* step = kRotateOrders[axis]; *step != 0;) {
        const char character = *step++;
        if (!IsAxis(character)) continue;
        const float a = *angle++;
        const float shifted = kHalfPi + a;
        // (int) of a float is fctiwz; the table is gTPGlobals.mSinTableMask + 1 entries long.
        const int32_t sinIndex = ppc_fctiwz(static_cast<double>(a * gTPGlobals.mSinTableScale));
        const int32_t cosIndex = ppc_fctiwz(static_cast<double>(shifted * gTPGlobals.mSinTableScale));
        const float s = gTPGlobals.mSinTable[static_cast<uint32_t>(sinIndex) & gTPGlobals.mSinTableMask];
        const float c = gTPGlobals.mSinTable[static_cast<uint32_t>(cosIndex) & gTPGlobals.mSinTableMask];
        RotateAbout(matrix, character, c, s);
    }
}

void RotateLMatrixXYZNoTransHi(sMatrix* matrix, eRotateParams axis, float* angle) {
    for (const char* step = kRotateOrders[axis]; *step != 0;) {
        const char character = *step++;
        if (!IsAxis(character)) continue;
        const float a = *angle++;
        const float c = static_cast<float>(std::cos(static_cast<double>(a)));
        const float s = static_cast<float>(std::sin(static_cast<double>(a)));
        RotateAbout(matrix, character, c, s);
    }
}

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

namespace {

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

}  // namespace

// 0x100c1ab8, 328 bytes. Row 0 gets the length x, row 1 y and row 2 z; the translation is not touched.
void SetLMatrixScaleNoTrans(sMatrix* matrix, float& x, float& y, float& z) {
    ScaleRow(matrix->m[0], x);
    ScaleRow(matrix->m[1], y);
    ScaleRow(matrix->m[2], z);
}

// Each row of the 3 x 3 part times its axis' scale (x for row 0, y for row 1, z for row 2): the rotation part of a node's
// matrix, in single precision, one multiply each. The translation row is left alone.
void ScaleLMatrixNoTrans(sMatrix* matrix, s3DPoint* scale) {
    matrix->m[0][0] = matrix->m[0][0] * scale->x;
    matrix->m[0][1] = matrix->m[0][1] * scale->x;
    matrix->m[0][2] = matrix->m[0][2] * scale->x;
    matrix->m[1][0] = matrix->m[1][0] * scale->y;
    matrix->m[1][1] = matrix->m[1][1] * scale->y;
    matrix->m[1][2] = matrix->m[1][2] * scale->y;
    matrix->m[2][0] = matrix->m[2][0] * scale->z;
    matrix->m[2][1] = matrix->m[2][1] * scale->z;
    matrix->m[2][2] = matrix->m[2][2] * scale->z;
}

// 0x100c1d0c, 112 bytes: the overload that takes the three scales as floats (the s3DPoint form is 0x100c1c3c in rsse_matrix.cpp).
void ScaleLMatrixNoTrans(sMatrix* matrix, float x, float y, float z) {
    matrix->m[0][0] = matrix->m[0][0] * x;
    matrix->m[0][1] = matrix->m[0][1] * x;
    matrix->m[0][2] = matrix->m[0][2] * x;
    matrix->m[1][0] = matrix->m[1][0] * y;
    matrix->m[1][1] = matrix->m[1][1] * y;
    matrix->m[1][2] = matrix->m[1][2] * y;
    matrix->m[2][0] = matrix->m[2][0] * z;
    matrix->m[2][1] = matrix->m[2][1] * z;
    matrix->m[2][2] = matrix->m[2][2] * z;
}

// rsse.cpp (main app of SimTheme Park 2.04b3): SetLMatrixOrientationHi (0x100c1db4, 192 bytes), unit w135.

// The 3 x 3 part is built in two steps. The pitch about X first: row 0 is (1, 0, 0), row 1 (0, cos, sin) and row 2 (0, -sin, cos), each
// of the C library's sin and cos of the pitch (as doubles) rounded to single, the negation done on the single. Row 3 and the fourth
// column are not written. Then the yaw: YRotateLMatrixNoTrans(matrix, cos(yaw), sin(yaw)) with the same rounding.
void SetLMatrixOrientationHi(sMatrix* matrix, float pitch, float yaw) {
    const float sp = static_cast<float>(std::sin(static_cast<double>(pitch)));
    const float cp = static_cast<float>(std::cos(static_cast<double>(pitch)));
    matrix->m[2][0] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][0] = 1.0f;
    matrix->m[1][1] = cp;
    matrix->m[1][2] = sp;
    matrix->m[2][1] = -sp;
    matrix->m[2][2] = cp;
    const float sy = static_cast<float>(std::sin(static_cast<double>(yaw)));
    const float cy = static_cast<float>(std::cos(static_cast<double>(yaw)));
    YRotateLMatrixNoTrans(matrix, cy, sy);
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

// The point through the matrix's upper 3x3 and its translation row; the point is rewritten
// in place. Each coordinate is one single-precision product, two fused multiply-adds and
// the translation added last (the original reads x, y and z into registers first).
void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point) {
    const float x = point->x;
    const float y = point->y;
    const float z = point->z;
    float sum = y * matrix->m[1][0];
    sum = ppc_fmadds(x, matrix->m[0][0], sum);
    sum = ppc_fmadds(z, matrix->m[2][0], sum);
    point->x = matrix->m[3][0] + sum;
    sum = y * matrix->m[1][1];
    sum = ppc_fmadds(x, matrix->m[0][1], sum);
    sum = ppc_fmadds(z, matrix->m[2][1], sum);
    point->y = matrix->m[3][1] + sum;
    sum = y * matrix->m[1][2];
    sum = ppc_fmadds(x, matrix->m[0][2], sum);
    sum = ppc_fmadds(z, matrix->m[2][2], sum);
    point->z = matrix->m[3][2] + sum;
}

// 0x100c2040, 132 bytes: the point through the 3 x 3 part (no translation, no w), in place: each coordinate is y times the second
// row, then x times the first row and z times the third fused in.
void VectorThroughLMatrixNoWNoTrans(sMatrix* matrix, s3DPoint* point) {
    const float x = point->x;
    const float y = point->y;
    const float z = point->z;
    point->x = ppc_fmadds(z, matrix->m[2][0], ppc_fmadds(x, matrix->m[0][0], y * matrix->m[1][0]));
    point->y = ppc_fmadds(z, matrix->m[2][1], ppc_fmadds(x, matrix->m[0][1], y * matrix->m[1][1]));
    point->z = ppc_fmadds(z, matrix->m[2][2], ppc_fmadds(x, matrix->m[0][2], y * matrix->m[1][2]));
}

void LUBackSubst(sMatrix& matrix, int32_t* indices, float* values) {
    int32_t first = -1;
    for (int32_t i = 0; i < 4; ++i) {
        float sum = values[indices[i]];
        values[indices[i]] = values[i];
        if (first < 0) {
            if (sum != 0.0f) first = i;
        } else {
            for (int32_t j = first; j < i; ++j) sum = sum - values[j] * matrix.m[i][j];
        }
        values[i] = sum;
    }
    for (int32_t i = 3; i >= 0; --i) {
        float sum = values[i];
        for (int32_t j = i + 1; j < 4; ++j) sum = sum - values[j] * matrix.m[i][j];
        values[i] = sum / matrix.m[i][i];
    }
}

void LUDecomp(sMatrix& matrix, int32_t* indices, float* parity) {
    float scale[4];
    int32_t pivot = 0;
    *parity = 1.0f;
    for (int32_t i = 0; i < 4; ++i) {
        float largest = 0.0f;
        for (int32_t j = 0; j < 4; ++j)
            if (largest < std::fabs(matrix.m[i][j])) largest = std::fabs(matrix.m[i][j]);
        scale[i] = 1.0f / largest;
    }
    for (int32_t j = 0; j < 4; ++j) {
        for (int32_t i = 0; i < j; ++i) {
            float sum = matrix.m[i][j];
            for (int32_t k = 0; k < i; ++k) sum = ppc_fnmsubs(matrix.m[i][k], matrix.m[k][j], sum);
            matrix.m[i][j] = sum;
        }
        float largest = 0.0f;
        for (int32_t i = j; i < 4; ++i) {
            float sum = matrix.m[i][j];
            for (int32_t k = 0; k < j; ++k) sum = ppc_fnmsubs(matrix.m[i][k], matrix.m[k][j], sum);
            matrix.m[i][j] = sum;
            const float weighted = scale[i] * std::fabs(sum);
            if (largest <= weighted) { largest = weighted; pivot = i; }
        }
        if (j != pivot) {
            for (int32_t k = 0; k < 4; ++k) std::swap(matrix.m[pivot][k], matrix.m[j][k]);
            *parity = -*parity;
            scale[pivot] = scale[j];
        }
        indices[j] = pivot;
        if (matrix.m[j][j] == 0.0f) matrix.m[j][j] = 1.0e-20f;
        if (j != 3) {
            const float reciprocal = 1.0f / matrix.m[j][j];
            for (int32_t i = j + 1; i < 4; ++i) matrix.m[i][j] *= reciprocal;
        }
    }
}

void InvertMatrix(sMatrix* destination, const sMatrix* source) {
    sMatrix decomposed = *source;
    sMatrix inverse;
    int32_t indices[4];
    float parity;
    LUDecomp(decomposed, indices, &parity);
    for (int32_t j = 0; j < 4; ++j) {
        float column[4] = {};
        column[j] = 1.0f;
        LUBackSubst(decomposed, indices, column);
        for (int32_t i = 0; i < 4; ++i) inverse.m[i][j] = column[i];
    }
    *destination = inverse;
}

// The UI's matrix helpers (main app of SimTheme Park 2.04b3): GetUIMatrix (0x10187ce4, 848 bytes, the ui_utils.cpp range that holds the engine
// mesh draw objects) and the matrix combiner CombineMatricesNoWDest1st (0x100c2694, 548 bytes, the rsse matrix code), which GetUIMatrix and the
// engine mesh object's Change use. See include/ui_DrawObject.h and include/rsse.h.

// destination = destination * source for the first three columns of all four rows (the fourth column is not made). The destination is copied
// first; then for each row and column the product of the copy's element 1 and the source's, fused with elements 0 and 2, and for row 3 the
// source's translation added last. Elements are stored one by one and the source is read again for each, so a source that is the destination
// changes under the loop (the callers never do that).
void CombineMatricesNoWDest1st(sMatrix* destination, sMatrix* source) {
    const sMatrix old = *destination;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            float sum = old.m[row][1] * source->m[1][column];
            sum = ppc_fmadds(old.m[row][0], source->m[0][column], sum);
            sum = ppc_fmadds(old.m[row][2], source->m[2][column], sum);
            if (row == 3) sum = source->m[3][column] + sum;
            destination->m[row][column] = sum;
        }
    }
}

// destination = source * destination, without the fourth column: for each of the first
// three columns and each row, source[row][1] * old[1][col], then fused source[row][0] *
// old[0][col] and source[row][2] * old[2][col]; row 3 adds old[3][col] last. The
// destination is copied first, the results are stored column by column, and the source is
// read again for every element (so a source that is the destination changes under it).
void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) {
    const sMatrix old = *destination;
    for (int32_t column = 0; column < 3; ++column) {
        for (int32_t row = 0; row < 4; ++row) {
            float sum = source->m[row][1] * old.m[1][column];
            sum = ppc_fmadds(source->m[row][0], old.m[0][column], sum);
            sum = ppc_fmadds(source->m[row][2], old.m[2][column], sum);
            if (row == 3) sum = old.m[3][column] + sum;
            destination->m[row][column] = sum;
        }
    }
}

// 0x100c2b5c, 428 bytes: second = first * second for the 3 x 3 part (row 3 and the fourth column of second are left alone). The
// original copies the whole second matrix to the stack first, so the second matrix may change under the loop; the FIRST matrix is read
// live while the elements are stored, so a first that is the second changes under it (column by column, rows 0 to 2 of each).
void CombineMatricesNoWNoTransDest2nd(sMatrix* first, sMatrix* second) {
    float s[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            s[i][j] = second->m[i][j];
        }
    }
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            second->m[i][j] = ppc_fmadds(first->m[i][2], s[2][j], ppc_fmadds(first->m[i][0], s[0][j], first->m[i][1] * s[1][j]));
        }
    }
}

// 0x100c2d50, 360 bytes: a recursive merge sort of `count` object pointers by the float at byte `offset` of each object. The two arrays
// swap roles with the depth: at an even depth the sorted result goes to `scratch` and the halves are read from `source`; at an odd depth
// it goes to `source` and the halves are read from `scratch`. A count of 1 (or any count below 0) copies source[0] to the destination
// (a no-op at an odd depth: the destination is the source); 0 does nothing. The halves are count / 2 (toward zero) and the rest, each
// sorted one depth deeper, then merged: while both halves have elements the left one is taken when its key is <= the right one's
// (fcmpo, then cror lt|eq), so equal keys keep their order and a NaN on either side takes the RIGHT one; when one half is empty the
// other's element is taken (the right half is tested first). The original's pointers are 32 bits; these are native.
void FloatSortByArray(int32_t count, void** source, void** scratch, int32_t depth, int32_t offset) {
    void** destination = (depth & 1) ? source : scratch;
    void** left = (depth & 1) ? scratch : source;
    if (count <= 1) {
        if (count != 0) {
            *destination = *source;
        }
        return;
    }
    const int32_t leftCount = count / 2;
    const int32_t rightCount = count - leftCount;
    void** right = left + leftCount;
    FloatSortByArray(leftCount, source, scratch, depth + 1, offset);
    FloatSortByArray(rightCount, source + leftCount, scratch + leftCount, depth + 1, offset);
    int32_t leftLeft = leftCount;
    int32_t rightLeft = rightCount;
    for (int32_t n = count; n != 0; n--) {
        if (leftLeft != 0 && rightLeft != 0) {
            void* a = *left;
            void* b = *right;
            const float keyA = *reinterpret_cast<const float*>(static_cast<const char*>(a) + offset);
            const float keyB = *reinterpret_cast<const float*>(static_cast<const char*>(b) + offset);
            if (keyA <= keyB) {
                *destination = a;
                left++;
                leftLeft--;
            } else {
                *destination = b;
                right++;
                rightLeft--;
            }
        } else if (rightLeft != 0) {
            *destination = *right;
            right++;
            rightLeft--;
        } else {
            *destination = *left;
            left++;
            leftLeft--;
        }
        destination++;
    }
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

// The rsse.cpp range of the inventory (decompiled from the main app of SimTheme Park 2.04b3).
// PARTIAL: only BuildGeometryTables (0x100c30a8) is translated so far; see rsse.h for the
// rest of the range.

namespace {
constexpr float kTwoPi = 6.2831855f;   // 0x40c90fdb
}

void BuildGeometryTables(uint32_t size) {
    gTPGlobals.mSinTableSize = size;
    gTPGlobals.mSinTableMask = size - 1;
    gTPGlobals.mSinTableScale = static_cast<float>(size) / kTwoPi;
    for (uint32_t i = 0; i < gTPGlobals.mSinTableSize; ++i) {
        const float angle = kTwoPi * (static_cast<float>(i) / static_cast<float>(gTPGlobals.mSinTableSize));
        gTPGlobals.mSinTable[i] = static_cast<float>(std::sin(static_cast<double>(angle)));
    }
}

// 0x100c3198, 92 bytes: sqrt of dx*dx + dy*dy + dz*dz. The differences and dy*dy, dz*dz are single precision (fsubs, fmuls); dx*dx is
// fused (fmadd, double) with dy*dy, dz*dz is added (fadd, double); the root is the C library's, rounded to single.
float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    const float dy = a->y - b->y;
    const float dx = a->x - b->x;
    const float dz = a->z - b->z;
    double r = ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(dy * dy));
    r = static_cast<double>(dz * dz) + r;
    return static_cast<float>(std::sqrt(r));
}
