// The dynamic face list and the water (analyser.cpp of the main app; unit w127): sort (0x100dfb68, 512 bytes),
// DC_ResetSquareFaceListPtr (0x100e0260, 16), DC_AddSquareFaceToList (0x100e02a0, 268), DC_AddPolyFaceToList (0x100e03f0, 256),
// HDZ_RotatePointZ (0x100e0544, 148), DC_DrawWater (0x100e060c, 580) and DC_AnimateWater (0x100e10f4, 560).
//
// The face list is gDynamicFaces (0xa0-byte records, src/DynamicCells.cpp) with gDynamicFaceKeys naming them; the two adders append to it
// and the tick (DC_TickDynamicCellMeshes) clears it, sorts the keys by texture with `sort` and draws it with CDynamicMesh::DrawAll.
// DC_AnimateWater fills two 16 x 19 grids of points (the surface normals and the positions of the water) and DC_DrawWater turns them into
// one poly face per cell, 10 x 11 and 4 x 6 cells of the grids with an offset (the tick's two calls), each with a texture from
// gDynamicCellRandom. Every sine of the area is the engine's table: gTPGlobals.mSinTable[mask & (int)(angle * scale)].
#include "DynamicMesh.h"

#include <cmath>

#include "Debug.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "m3d2.h"
#include "ppc_math.h"

// ---- the grids (BSS on the Mac: zero at start). The column stride is 19 points (0xe4 bytes), the row stride one point.
s3DPoint gDynamicGridNormals[16][19];     // 0x102f7ec8
s3DPoint gDynamicGridPositions[16][19];   // 0x102f8d08

namespace {

constexpr float kHalfPi = 1.5707964f;       // 0x3fc90fdb (0x1026504c): the cosine is the sine of this much further on
constexpr float kWaveTurn = 2.0943949f;     // 0x40060a91 (0x10265044): 2 pi / 3
constexpr float kWaveA = 0.17453299f;       // 0x3e32b8c7 (0x10265040)
constexpr float kWaveB = 1.7453294f;        // 0x3fdf66f4 (0x1026503c)
constexpr float kWaveHeight = 1.7f;         // 0x3fd9999a (0x10265038)

// The engine's sine table read at the angle's index (fctiwz of angle * scale, masked).
inline float SineAt(float scaledAngle) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(scaledAngle))];
}

}  // namespace

// 0x100dfb68: the Numerical Recipes quicksort on 1-based arrays, comparing the low five bits only (so equal keys end in no fixed order),
// with insertion sort for ranges of fewer than 7 more than the first. The pairs of the explicit stack are (first, last) of the range left
// to do, the larger part pushed. The insertion loop stops at index 0, not at the range's start (the data left of the range is never greater).
void sort(uint32_t* keysMinusOne, uint32_t count) {
    uint32_t* arr = keysMinusOne;
    uint32_t stack[54];   // the original's frame words (r1 - 0xe4 on); the pair of depth jstack is stack[jstack] (first) and [jstack + 1] (last)
    uint32_t l = 1;
    uint32_t ir = count;
    uint32_t jstack = 0;
    for (;;) {
        if (ir - l < 7) {
            for (uint32_t j = l + 1; j <= ir; ++j) {
                const uint32_t a = arr[j];
                uint32_t i = j - 1;
                while (i >= 1) {
                    const uint32_t value = arr[i];
                    if ((value & 0x1f) <= (a & 0x1f)) break;
                    arr[i + 1] = value;
                    --i;
                }
                arr[i + 1] = a;
            }
            if (jstack == 0) return;
            ir = stack[jstack + 1];
            l = stack[jstack];
            jstack -= 2;
        } else {
            uint32_t* const middle = arr + ((l + ir) >> 1);
            uint32_t* const second = arr + l + 1;
            uint32_t* const first = arr + l;
            uint32_t* const last = arr + ir;
            // The median of three: the middle goes to l + 1, then the three are put in order by their low five bits.
            const uint32_t moved = *middle;
            *middle = *second;
            *second = moved;
            uint32_t x = *first;
            uint32_t y = *last;
            if ((x & 0x1f) > (y & 0x1f)) {
                *first = y;
                *last = x;
            }
            x = *second;
            y = *last;
            if ((x & 0x1f) > (y & 0x1f)) {
                *second = y;
                *last = x;
            }
            x = *first;
            y = *second;
            if ((x & 0x1f) > (y & 0x1f)) {
                *first = y;
                *second = x;
            }
            const uint32_t pivot = *second;
            const uint32_t pivotKey = pivot & 0x1f;
            uint32_t i = l + 1;
            uint32_t j = ir;
            uint32_t atI;
            uint32_t atJ;
            for (;;) {
                do {
                    ++i;
                    atI = arr[i];
                } while ((atI & 0x1f) < pivotKey);
                do {
                    --j;
                    atJ = arr[j];
                } while ((atJ & 0x1f) > pivotKey);
                if (j < i) break;
                arr[i] = atJ;
                arr[j] = atI;
            }
            *second = atJ;
            arr[j] = pivot;
            jstack += 2;
            if (ir - i + 1 < j - l) {
                stack[jstack + 1] = j - 1;
                stack[jstack] = l;
                l = i;
            } else {
                stack[jstack + 1] = ir;
                stack[jstack] = i;
                ir = j - 1;
            }
        }
    }
}

// 0x100e0260: empties the face list.
void DC_ResetSquareFaceListPtr() {
    gDynamicCellFBDBC = 0;
}

// 0x100e02a0
void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t unused2, int32_t keepPosition, DYN_Type type) {
    const int32_t count = gDynamicCellFBDBC;
    if (count >= kDynamicFaceMax) {
        CTRACE(6, "Too many (square) dynamic faces !\n");
        return;
    }
    // A face of texture 1 is dropped while the flag byte is 0 (the original tests the flag only for that texture).
    if (texture == 1 && gDynamicCellFlag == 0) return;
    sDynamicFace& face = gDynamicFaces[count];
    face.mX = x;
    face.mY = y;
    face.mZ = z;
    face.mSide = side;
    face.mRotation = rotation;
    face.mF14 = unused1;
    face.mLarge = large;
    face.mF1C = unused2;
    face.mTexture = static_cast<uint8_t>(texture);
    face.mIsSquare = 1;
    face.mKeepPosition = gDynamicCellF9BD4 == 0 ? keepPosition : 1;
    face.mType = type;
    gDynamicFaceKeys[count] = (static_cast<uint32_t>(count) << 5) | (static_cast<uint32_t>(texture) & 0x1f);
    gDynamicCellFBDBC = gDynamicCellFBDBC + 1;
}

// 0x100e03f0
void DC_AddPolyFaceToList(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points, DYN_Type type) {
    const int32_t count = gDynamicCellFBDBC;
    if (count >= kDynamicFaceMax) {
        CTRACE(6, "Too many (poly) dynamic faces !\n");
        return;
    }
    sDynamicFace& face = gDynamicFaces[count];
    for (int32_t k = 0; k < 4; ++k) {
        face.mPositions[k].x = positions[k].x;
        face.mPositions[k].y = positions[k].y;
        face.mPositions[k].z = positions[k].z;
        face.mUVs[k].x = uvs[k].x;       // the texture coordinates' z is not copied
        face.mUVs[k].y = uvs[k].y;
        face.mPoints[k].x = points[k].x;
        face.mPoints[k].y = points[k].y;
        face.mPoints[k].z = points[k].z;
    }
    face.mTexture = static_cast<uint8_t>(texture);
    face.mIsSquare = 0;
    face.mKeepPosition = 0;
    face.mType = type;
    gDynamicFaceKeys[count] = (static_cast<uint32_t>(count) << 5) | (static_cast<uint32_t>(texture) & 0x1f);
    gDynamicCellFBDBC = gDynamicCellFBDBC + 1;
}

// 0x100e0544: turns x and y of four consecutive points about z by the angle (z untouched). The sine and the cosine are read once; each
// point's new y and new x are fused multiply-adds on the old values.
void HDZ_RotatePointZ(s3DPoint* points, float angle) {
    const float shifted = kHalfPi + angle;
    const float scale = gTPGlobals.mSinTableScale;
    const float scaledAngle = angle * scale;
    const float scaledShifted = shifted * scale;
    const float sine = SineAt(scaledAngle);
    const float cosine = SineAt(scaledShifted);
    for (int32_t k = 0; k < 4; ++k) {
        const float oldX = points[k].x;
        const float oldY = points[k].y;
        points[k].y = ppc_fmadds(oldY, cosine, oldX * sine);
        points[k].x = ppc_fmadds(-oldY, sine, points[k].x * cosine);
    }
}

// 0x100e060c: one poly face of the water (type 1) per cell of a width x height block of the grids, from column colOffset and row rowOffset:
// the four corners of the cell (the cell's column, then the next, in the row, then in the next row) with (x, y, z) added to the positions,
// the unit square's texture coordinates, the normals of the corners and the texture gDynamicCellRandom names for the cell (counted from 0
// over the whole call). `e` and `f` are not read; the original has the same call in both arms of a test of colOffset.
void DC_DrawWater(float x, float y, float z, int32_t width, int32_t height, int32_t colOffset, int32_t rowOffset, int32_t e, int32_t f) {
    s3DPoint uvs[4] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    int32_t cell = 0;
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t column = 0; column < width; ++column) {
            const int32_t c0 = column + colOffset;
            const int32_t r0 = row + rowOffset;
            // Corner k of the cell: column c0 + (k & 1), row r0 + (k >> 1).
            const s3DPoint* const base[4] = {&gDynamicGridPositions[c0][r0], &gDynamicGridPositions[c0 + 1][r0],
                                             &gDynamicGridPositions[c0][r0 + 1], &gDynamicGridPositions[c0 + 1][r0 + 1]};
            const s3DPoint* const normal[4] = {&gDynamicGridNormals[c0][r0], &gDynamicGridNormals[c0 + 1][r0],
                                               &gDynamicGridNormals[c0][r0 + 1], &gDynamicGridNormals[c0 + 1][r0 + 1]};
            s3DPoint positions[4];
            s3DPoint points[4];
            for (int32_t k = 0; k < 4; ++k) {
                positions[k].x = base[k]->x + x;
                positions[k].y = base[k]->y + y;
                positions[k].z = base[k]->z + z;
                points[k].x = normal[k]->x;
                points[k].y = normal[k]->y;
                points[k].z = normal[k]->z;
            }
            DC_AddPolyFaceToList(positions, static_cast<int32_t>(gDynamicCellRandom[cell]), uvs, points, kDynWater);
            ++cell;
        }
    }
}

// 0x100e10f4: the water's surface for `columns` x `rows` points `step` apart: a wave (a sum of two angles into the sine table, moved on by
// `phase`) gives each point's height y = 1.7 * cosine and its normal (s, 1, s) / length with s the wave's sine, the length rounded to
// single. Point [column][row] of the positions grid is (column * step, height, row * step). Single-precision fused multiply-adds where the
// listing has them; the squared lengths are summed in double (x squared in double, y and z squared rounded to single first) and the root
// is the C library's.
void DC_AnimateWater(int32_t columns, int32_t rows, float step, float phase) {
    const float zero = 0.0f;    // 0x10265088
    const float one = 1.0f;     // 0x10265074
    const float half = 0.5f;    // 0x10265070
    const float scale = gTPGlobals.mSinTableScale;
    const float scaledA = kWaveA * scale;
    const float scaledB = kWaveB * scale;
    const float sineA = SineAt(scaledA);
    const float sineB = SineAt(scaledB);
    const float zeroTimesA = zero * sineA;
    const float zeroTimesB = zero * sineB;
    const float turnColumn = ppc_fmadds(kWaveTurn, sineB, zeroTimesA);     // f31: the wave's change per column
    const float turnRow = ppc_fmadds(-kWaveTurn, sineA, zeroTimesB);       // f30: the wave's change per row
    float rowCoord = zero;
    for (int32_t row = 0; row < rows; ++row) {
        float columnCoord = zero;
        for (int32_t column = 0; column < columns; ++column) {
            s3DPoint& position = gDynamicGridPositions[column][row];
            s3DPoint& normal = gDynamicGridNormals[column][row];
            float angle = ppc_fmadds(turnColumn, columnCoord, zero);
            angle = ppc_fmadds(turnRow, rowCoord, angle);
            position.x = columnCoord;
            const float wave = ppc_fnmsubs(half, phase, angle);     // angle - 0.5 * phase
            const float shifted = kHalfPi + wave;
            const float sineScaled = wave * gTPGlobals.mSinTableScale;
            const float cosineScaled = shifted * gTPGlobals.mSinTableScale;
            const int32_t sineIndex = ppc_fctiwz(sineScaled);
            const int32_t cosineIndex = ppc_fctiwz(cosineScaled);
            position.y = kWaveHeight * gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(cosineIndex)];
            position.z = rowCoord;
            const float negSine = -gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(sineIndex)];
            const double zeroSquared = static_cast<double>(zero) * static_cast<double>(zero);
            const double scaledNeg = static_cast<double>(one) * static_cast<double>(negSine);
            normal.x = static_cast<float>(ppc_fmsub(static_cast<double>(negSine), static_cast<double>(zero), scaledNeg));
            normal.y = static_cast<float>(ppc_fmsub(static_cast<double>(one), static_cast<double>(one), zeroSquared));
            normal.z = static_cast<float>(ppc_fmsub(static_cast<double>(zero), static_cast<double>(negSine), scaledNeg));
            const double xx = static_cast<double>(normal.x) * static_cast<double>(normal.x);
            const float yy = normal.y * normal.y;
            const float zz = normal.z * normal.z;
            const double sum = static_cast<double>(zz) + (xx + static_cast<double>(yy));
            const float length = static_cast<float>(std::sqrt(sum));
            columnCoord = columnCoord + step;
            normal.x = normal.x / length;
            normal.y = normal.y / length;
            normal.z = normal.z / length;
        }
        rowCoord = rowCoord + step;
    }
}
