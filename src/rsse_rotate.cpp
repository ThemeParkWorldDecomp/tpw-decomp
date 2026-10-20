// rsse.cpp (main app of SimTheme Park 2.04b3): turning a matrix's 3 x 3 part about the axes (X/Y/ZRotateLMatrixNoTrans
// 0x100c13bc, 0x100c1698, 0x100c15f0, RotateLMatrixXYZNoTrans 0x100c174c, RotateLMatrixXYZNoTransHi 0x100c1944).
// Each row of the matrix is turned on its own (row 3, the translation, is not), as the row vector times the rotation;
// every product is first rounded to single and the sum or difference is fused with the product before it.
#include <cmath>

#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

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
