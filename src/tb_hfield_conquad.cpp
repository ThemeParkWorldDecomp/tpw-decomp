// tb_hfield_conquad.cpp (engine library of SimTheme Park 2.04b3; the inventory's tb_hfield.cpp
// range): CHField::TestConQuad 0x1000dd14 (1,024 bytes).
//
// A first-person view (the lobby's) finds the cells it sees with CHField::DivideAndConquer, which
// hands boxes of grid cells to TestConQuad. A box is the cells x0..x1 by z0..z1; TestConQuad
// builds the eight corners of the block of space above it (the height field's lowest and highest
// heights over the box's four corners), transforms them by the final matrix and asks the clipper
// for their outside codes. When all eight lie outside one plane the box is out of view and
// nothing happens. When all eight are inside, or the box is a single cell, the rows of grid
// points at +0x20 are widened to take the box and mF1C (the far plane the projection will use)
// is raised to the farthest depth the corners reached. Otherwise the box is cut in four at the
// middle and each part is tested in turn.
#include "tb_hfield.h"

#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"

void CHField::TestConQuad(int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    const sHField& field = *mF00;
    // The cell corners in world space: the position is fused (cell * size + origin), the cell
    // number is converted to single precision first. mF08 is the origin x, z (0, 1) and the
    // cell size x, z (2, 3).
    const float worldX0 = ppc_fmadds(static_cast<float>(x0), field.mF08[2], field.mF08[0]);
    const float worldZ0 = ppc_fmadds(static_cast<float>(z0), field.mF08[3], field.mF08[1]);
    const float worldX1 = ppc_fmadds(static_cast<float>(x1), field.mF08[2], field.mF08[0]);
    const float worldZ1 = ppc_fmadds(static_cast<float>(z1), field.mF08[3], field.mF08[1]);
    const float low = field.mF20[0];
    const float high = field.mF20[1];
    // The eight corners, x, y, z each: the lowest height first (z0 then z1 rows), then the highest.
    float corners[8][3] = {
        {worldX0, low, worldZ0},  {worldX1, low, worldZ0},  {worldX0, low, worldZ1},  {worldX1, low, worldZ1},
        {worldX0, high, worldZ0}, {worldX1, high, worldZ0}, {worldX0, high, worldZ1}, {worldX1, high, worldZ1},
    };
    // The product of the box's two extents, 32 bits: 1 is a single cell.
    const int32_t extent = static_cast<int32_t>(static_cast<uint32_t>(x1 - x0) * static_cast<uint32_t>(z1 - z0));

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    SVertex transformed[8];
    gei_MathUnit->Transform(gei_FinalMatrix, transformed, corners, 0x10, 0xc, 8, 0);
    // The clipper's answer: [0] is the AND of the eight outside codes, [1] their OR.
    uint8_t answer[4] = {0xff, 0, 0, 0};
    uint8_t codes[8];
    CClipping::CPP_TestData(answer, gei_Clipping, codes, transformed, 8, 0x10, 0xff);
    if (answer[0] != 0) return;

    if (answer[1] == 0 || extent == 1) {
        // In view whole, or too small to cut: the rows z0..z1 reach out to the box (the columns
        // are bytes in the object, compared as ints against the full column number).
        SGamutQuads* rows = VertexRows();
        if (mF20 > z0) mF20 = z0;
        if (mF24 < z1) mF24 = z1;
        for (int32_t z = z0; z <= z1; ++z) {
            if (rows->mLow[z] > x0) rows->mLow[z] = static_cast<int8_t>(x0);
            if (rows->mHigh[z] < x1) rows->mHigh[z] = static_cast<int8_t>(x1);
        }
        // mF1C follows the depth (the third word) of each corner, farthest wins; a NaN never does.
        for (const SVertex& corner : transformed) {
            if (mF1C < corner.z) mF1C = corner.z;
        }
        return;
    }

    // Partly in view: the four quarters, each only when it has some width and depth.
    const int32_t middleX = static_cast<int32_t>(static_cast<uint32_t>(x0) + static_cast<uint32_t>(x1)) >> 1;
    const int32_t middleZ = static_cast<int32_t>(static_cast<uint32_t>(z0) + static_cast<uint32_t>(z1)) >> 1;
    if (middleX > x0) {
        if (middleZ > z0) TestConQuad(x0, z0, middleX, middleZ);
        if (z1 > middleZ) TestConQuad(x0, middleZ, middleX, z1);
    }
    if (x1 > middleX) {
        if (middleZ > z0) TestConQuad(middleX, z0, x1, middleZ);
        if (z1 > middleZ) TestConQuad(middleX, middleZ, x1, z1);
    }
}
