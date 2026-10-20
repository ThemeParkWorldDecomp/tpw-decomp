// The UI's matrix helpers (main app of SimTheme Park 2.04b3): GetUIMatrix (0x10187ce4, 848 bytes, the ui_utils.cpp range that holds the engine
// mesh draw objects) and the matrix combiner CombineMatricesNoWDest1st (0x100c2694, 548 bytes, the rsse matrix code), which GetUIMatrix and the
// engine mesh object's Change use. See include/ui_DrawObject.h and include/rsse.h.
#include <cstdint>

#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "ui_DrawObject.h"

namespace {

// The constants the listing loads (0x10265460..0x10265488 and the data words 0x102a7af0, 0x102a7af4).
constexpr float kPi = 3.14159274f;           // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;       // 0x3fc90fdb
constexpr float kUnitsPerTurn = 2048.0f;     // 0x45000000: the angle is in 1/4096ths of a turn, so 2048 of them are pi
constexpr float kHalfWidth = 1024.0f;        // 0x44800000: half the UI's 2048 units
constexpr float kHeight = 1536.0f;           // 0x44c00000
constexpr float kHalfHeight = 768.0f;        // 0x44400000
constexpr float kInverseWidth = 1.0f / 2048.0f;     // 0x3a000000 (data 0x102a7af0)
constexpr float kInverseHeight = 1.0f / 1536.0f;    // 0x3a2aaaab (data 0x102a7af4)
constexpr float kFour = 4.0f;                // 0x40800000
constexpr float kThree = 3.0f;               // 0x40400000
constexpr float kQuarter = 0.25f;            // 0x3e800000

}  // namespace

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

// The matrix that takes an object whose box (lower left corner x, y and size w, h in the mesh's units) is placed on the UI rectangle with the
// given left and BOTTOM edge and size (units of the 2048 x 1536 space) to the engine's screen. Five identity matrices are made, in this order:
// the rotation, the translation back from the pivot, the placement, the scale and the translation to the pivot.
//   placement: x = (left - 1024) * sx - x0 and y = (1536 - bottom - 768) * sy - y0, with sx, sy the mesh units per UI unit (w / width, h /
//              height); the products fuse with the subtractions as the listing has them;
//   scale:     (2 / sx) / 2048 and (2 / sy) / 1536 (the data words are 1/2048 and 1/1536), z 0;
//   rotation (only for a nonzero angle): the angle is pi * angle / 2048 radians; the sine and cosine come from TPGlobals' sine table at the
//              index (angle * scale, truncated) & mask, the cosine's from (pi/2 + angle) * scale. The 2 x 2 part is [cos, -4 sin / 3; 3 sin / 4,
//              cos] (the screen is 4:3). The pivot, in the UI's units from its centre, is moved to the origin by the pivot matrix (translation
//              -(pivotX - 1024) * sx, -(1536 - pivotY - 768) * sy) and back by the other (the same without the minus), both in the object's units.
// The result is placement * (pivot * rotation * back) * scale, copied whole (the fourth column is the identity's).
void GetUIMatrix(sMatrix* matrix, int16_t left, int16_t bottom, int16_t width, int16_t height, float x, float y, float w, float h,
                 int16_t pivotX, int16_t pivotY, int16_t angle) {
    sMatrix rotation;
    sMatrix back;
    sMatrix placement;
    sMatrix scale;
    sMatrix pivot;
    SetMatrixIdentity(&rotation);
    SetMatrixIdentity(&back);
    SetMatrixIdentity(&placement);
    SetMatrixIdentity(&scale);
    SetMatrixIdentity(&pivot);
    const float scaleX = w / static_cast<float>(width);
    const float scaleY = h / static_cast<float>(height);
    const float inverseX = 2.0f / scaleX;
    const float inverseY = 2.0f / scaleY;
    const float up = kHeight - static_cast<float>(bottom);
    placement.m[3][0] = ppc_fnmsubs(kHalfWidth, scaleX, ppc_fmsubs(static_cast<float>(left), scaleX, x));
    placement.m[3][1] = ppc_fnmsubs(kHalfHeight, scaleY, ppc_fmsubs(up, scaleY, y));
    scale.m[0][0] = inverseX * kInverseWidth;
    scale.m[1][1] = inverseY * kInverseHeight;
    scale.m[2][2] = 0.0f;
    if (angle != 0) {
        const float radians = (kPi * static_cast<float>(angle)) / kUnitsPerTurn;
        const uint32_t mask = gTPGlobals.mSinTableMask;
        const float turn = gTPGlobals.mSinTableScale;
        const float sine = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(ppc_fctiwz(radians * turn))];
        const float cosine = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(ppc_fctiwz((kHalfPi + radians) * turn))];
        const float offsetX = static_cast<float>(pivotX) - kHalfWidth;
        const float offsetY = (kHeight - static_cast<float>(pivotY)) - kHalfHeight;
        const float negativeX = ppc_fneg(offsetX);
        const float negativeY = ppc_fneg(offsetY);
        const float negativeSine = ppc_fneg(sine);
        const float threeSine = kThree * sine;
        pivot.m[3][0] = negativeX * scaleX;
        back.m[3][0] = offsetX * scaleX;
        pivot.m[3][1] = negativeY * scaleY;
        back.m[3][1] = offsetY * scaleY;
        rotation.m[0][0] = cosine;
        rotation.m[1][1] = cosine;
        rotation.m[0][1] = (kFour * negativeSine) / kThree;
        rotation.m[1][0] = threeSine * kQuarter;
        CombineMatricesNoWDest1st(&pivot, &rotation);
        CombineMatricesNoWDest1st(&pivot, &back);
    }
    CombineMatricesNoWDest1st(&placement, &pivot);
    CombineMatricesNoWDest1st(&placement, &scale);
    *matrix = placement;
}
