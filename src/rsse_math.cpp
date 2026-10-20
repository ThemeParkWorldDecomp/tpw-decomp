// rsse.cpp (main app of SimTheme Park 2.04b3): the matrix and vector helpers between 0x100c0eac and 0x100c3198 that had no translation
// (unit w170): identity of the 3 x 3 part and row 3, the angles of a matrix or a vector (atan2 of doubles rounded to single), the
// four-row axis rotations, a scale by three floats, a vector through the 3 x 3 part, the 3 x 3 product that writes its second
// argument, the distance of two points, and FloatSortByArray (a recursive merge sort of object pointers by a float member).
// The first fourteen came in from a translation made on a private test bench (puddle-challenges/q001) and were checked line by line
// against the listings; every fused call below is an fmadds, fmsubs or fmadd of the listing, every product that is not fused is an fmuls.
#include <cmath>
#include <cstdint>

#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

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

// 0x100c1040, 52 bytes: atan2(-y, z) of the point, in double precision rounded to single.
float GetVectorRotationAroundXAxisFromZ(s3DPoint& vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector.y), static_cast<double>(vector.z)));
}

// 0x100c10b8, 48 bytes: atan2(x, z).
float GetVectorRotationAroundYAxisFromZ(s3DPoint& vector) {
    return static_cast<float>(std::atan2(static_cast<double>(vector.x), static_cast<double>(vector.z)));
}

// 0x100c112c, 52 bytes: atan2(-x, y).
float GetVectorRotationAroundZAxisFromY(s3DPoint& vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector.x), static_cast<double>(vector.y)));
}

// 0x100c11a4, 52 bytes: the pointer form of 0x100c1040.
float GetVectorRotationAroundXAxisFromZ(s3DPoint* vector) {
    return static_cast<float>(std::atan2(-static_cast<double>(vector->y), static_cast<double>(vector->z)));
}

// 0x100c121c, 48 bytes: the pointer form of 0x100c10b8 (the one the original's callers use).
float GetVectorRotationAroundYAxisFromZ(s3DPoint* vector) {
    return static_cast<float>(std::atan2(static_cast<double>(vector->x), static_cast<double>(vector->z)));
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
