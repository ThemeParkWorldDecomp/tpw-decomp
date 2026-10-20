// The flags of the park (analyser.cpp of the main app; unit w128): DC_DrawFlag (0x100e087c, 2,124 bytes) and DC_AnimateFlag
// (0x100e1350, 664 bytes).
//
// The tick (DC_TickDynamicCellMeshes, src/DynamicCells.cpp) calls DC_AnimateFlag(7, 5, 1.8f, phase) once a frame and then DC_DrawFlag once per
// flag spot. DC_AnimateFlag fills the same two 16 x 19 grids as the water (src/DynamicFaces.cpp) for a 7 x 5 cloth: the positions grid with
// the points of the waving flag (x across, y down, z the ripple) and the normals grid with their normals. DC_DrawFlag turns the cloth into
// two poly faces (type kDynFlags, the texture the tick names) per cell of a columns x rows block, a back face with the normals negated and a
// front face with the normals as they are, turned about the y axis by the wind's angle and moved to the spot. Every sine of the area is the
// engine's table: gTPGlobals.mSinTable[mask & (int)(angle * scale)].
#include "DynamicMesh.h"

#include <cmath>

#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"

// 0x102a03a0 (TOC 0x10262c90): the largest fraction of the flag's width that DC_AnimateFlag lets the ripple grow with. Initialised data, 1.0f.
float gDynamicFlagClamp = 1.0f;

namespace {

constexpr float kHalfPi = 1.5707964f;       // 0x3fc90fdb (0x1026504c): the cosine is the sine of this much further on
constexpr float kHalfPiLow = 1.5707963f;    // 0x3fc90fda (0x10265030): one unit less in the last place; DC_AnimateFlag multiplies by it
constexpr float kWaveTurn = 2.0943949f;     // 0x40060a91 (0x10265044): 2 pi / 3
constexpr float kFlagSpeed = 0.6f;          // 0x3f19999a (0x10265034)
constexpr float kRipple = 0.3f;             // 0x3e99999a (0x10265028)
constexpr float kDroop = 0.02f;             // 0x3ca3d70a (0x10265024)
constexpr float kTextureHeight = 0.55f;     // 0x3f0ccccd (0x10265048): the v range of texture 0x13

// The engine's sine table read at the angle's index (fctiwz of angle * scale, masked).
inline float SineAt(float scaledAngle) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(scaledAngle))];
}

// The sine and cosine of `angle` as DC_DrawFlag reads them before each of its four turns (the scale and the mask are read again each time).
struct SineCosine {
    float mSine;
    float mCosine;
};

SineCosine SineCosineOf(float angle) {
    const float shifted = kHalfPi + angle;
    const float scale = gTPGlobals.mSinTableScale;
    const float scaledAngle = angle * scale;
    const float scaledShifted = shifted * scale;
    SineCosine result;
    result.mSine = SineAt(scaledAngle);
    result.mCosine = SineAt(scaledShifted);
    return result;
}

// Turns x and z of four consecutive points about y: z first (a fused multiply-add on the old values), then x on the old x.
void TurnAboutY(s3DPoint* points, const SineCosine& turn) {
    for (int32_t k = 0; k < 4; ++k) {
        const float oldX = points[k].x;
        const float oldZ = points[k].z;
        points[k].z = ppc_fmadds(oldZ, turn.mCosine, oldX * turn.mSine);
        points[k].x = ppc_fmadds(-oldZ, turn.mSine, points[k].x * turn.mCosine);
    }
}

// A grid position with `w` added to its x.
void CopyPosition(s3DPoint& to, const s3DPoint& from, float w) {
    to.x = from.x + w;
    to.y = from.y;
    to.z = from.z;
}

void CopyNormal(s3DPoint& to, const s3DPoint& from) {
    to.x = from.x;
    to.y = from.y;
    to.z = from.z;
}

void CopyNegated(s3DPoint& to, const s3DPoint& from) {
    to.x = -from.x;
    to.y = -from.y;
    to.z = -from.z;
}

}  // namespace

// 0x100e087c: the flag of columns x rows cells (7 x 5 points of the grids, the tick draws 6 x 4 cells) at (x, y, z): for each cell two poly
// faces of the texture, the cell's four corners in the positions grid with `w` added to their x (the cloth's offset), turned about y by
// `angle` and then moved by (x, y, z), with the normals of the corners (turned the same way, negated for the first face). The first face
// lists the corners (column + 1, row), (column, row), (column + 1, row + 1), (column, row + 1) with the texture coordinates u + du and u,
// the second lists (column, row), (column + 1, row), (column, row + 1), (column + 1, row + 1) with u and u + du; v starts at 1.0f (0.55f for
// texture 0x13) and falls by 0.55f / rows a row, u rises by 1 / columns a cell. The texture coordinates' z words are never written (the
// stack's leftovers on the Mac; DC_AddPolyFaceToList does not read them). The faces are of type kDynFlags (3).
void DC_DrawFlag(float x, float y, float z, float w, int32_t columns, int32_t rows, int32_t texture, float angle) {
    const float zero = 0.0f;                                        // 0x10265088
    const float one = 1.0f;                                         // 0x10265074
    const float du = one / static_cast<float>(columns);             // f28
    const float dv = kTextureHeight / static_cast<float>(rows);     // f27
    float v = one;                                                  // f25: texture 0x12 and every other texture but 0x13
    if (texture == 0x13) {
        v = kTextureHeight;
    }
    for (int32_t row = 0; row < rows; ++row) {
        float u = zero;     // f26
        for (int32_t column = 0; column < columns; ++column) {
            const s3DPoint& position00 = gDynamicGridPositions[column][row];
            const s3DPoint& position01 = gDynamicGridPositions[column][row + 1];
            const s3DPoint& position10 = gDynamicGridPositions[column + 1][row];
            const s3DPoint& position11 = gDynamicGridPositions[column + 1][row + 1];
            const s3DPoint& normal00 = gDynamicGridNormals[column][row];
            const s3DPoint& normal01 = gDynamicGridNormals[column][row + 1];
            const s3DPoint& normal10 = gDynamicGridNormals[column + 1][row];
            const s3DPoint& normal11 = gDynamicGridNormals[column + 1][row + 1];
            s3DPoint positions[4];
            s3DPoint normals[4];
            s3DPoint uvs[4] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

            // The first face: seen from behind.
            CopyPosition(positions[0], position10, w);
            CopyPosition(positions[1], position00, w);
            CopyPosition(positions[2], position11, w);
            CopyPosition(positions[3], position01, w);
            TurnAboutY(positions, SineCosineOf(angle));
            CopyNegated(normals[0], normal10);
            CopyNegated(normals[1], normal00);
            CopyNegated(normals[2], normal11);
            CopyNegated(normals[3], normal01);
            TurnAboutY(normals, SineCosineOf(angle));
            for (int32_t k = 0; k < 4; ++k) {
                positions[k].x = positions[k].x + x;
                positions[k].y = positions[k].y + y;
                positions[k].z = positions[k].z + z;
            }
            const float uNext = u + du;     // f30
            const float vNext = v - dv;     // f29
            uvs[0].x = uNext;
            uvs[0].y = v;
            uvs[1].x = u;
            uvs[1].y = v;
            uvs[2].x = uNext;
            uvs[2].y = vNext;
            uvs[3].x = u;
            uvs[3].y = vNext;
            DC_AddPolyFaceToList(positions, texture, uvs, normals, kDynFlags);

            // The second face: seen from the front.
            CopyPosition(positions[0], position00, w);
            CopyPosition(positions[1], position10, w);
            CopyPosition(positions[2], position01, w);
            CopyPosition(positions[3], position11, w);
            TurnAboutY(positions, SineCosineOf(angle));
            CopyNormal(normals[0], normal00);
            CopyNormal(normals[1], normal10);
            CopyNormal(normals[2], normal01);
            CopyNormal(normals[3], normal11);
            TurnAboutY(normals, SineCosineOf(angle));
            for (int32_t k = 0; k < 4; ++k) {
                positions[k].x = positions[k].x + x;
                positions[k].y = positions[k].y + y;
                positions[k].z = positions[k].z + z;
            }
            uvs[0].x = u;
            uvs[0].y = v;
            uvs[1].x = uNext;
            uvs[1].y = v;
            uvs[2].x = u;
            uvs[2].y = vNext;
            uvs[3].x = uNext;
            uvs[3].y = vNext;
            DC_AddPolyFaceToList(positions, texture, uvs, normals, kDynFlags);
            u = u + du;
        }
        v = v - dv;
    }
}

// 0x100e1350: the cloth of columns x rows points `step` apart, waving with `phase`: point [column][row] of the positions grid is
// (column * step, y, z) with a ripple (a sum of two sines of the distance from the flagpole at the top left, moved on by 0.6 * phase and
// scaled by how far across the flag the point is, up to the clamp) in y and z and a droop of 0.02 * x * x, and its normal is (-s, -s, 1)
// (s = the sine of the wave times minus the stretch, formed as the products with 0 and 1 the listing has, so a NaN or an infinity spreads
// the same way) divided by its length rounded to single. The distance is
// sqrt(x * x + (y - rows * step) ^ 2) with y the row's coordinate (0, -step, -2 step, ...: it falls by `step` a row). Fused multiply-adds
// where the listing has them; the squared lengths are summed in double (x squared in double, y and z squared rounded to single first) and the
// root is the C library's.
void DC_AnimateFlag(int32_t columns, int32_t rows, float step, float phase) {
    const float zero = 0.0f;    // 0x10265088
    const float one = 1.0f;     // 0x10265074
    const float two = 2.0f;     // 0x1026502c
    const float rowsSpan = static_cast<float>(rows) * step;     // f29
    float rowCoord = zero;                                      // f30: falls by `step` a row
    for (int32_t row = 0; row < rows; ++row) {
        float columnCoord = zero;                               // f31: rises by `step` a column
        for (int32_t column = 0; column < columns; ++column) {
            s3DPoint& position = gDynamicGridPositions[column][row];
            s3DPoint& normal = gDynamicGridNormals[column][row];
            const float rowOffset = rowCoord - rowsSpan;
            const float rowOffsetSquared = rowOffset * rowOffset;
            const float squaredDistance = ppc_fmadds(columnCoord, columnCoord, rowOffsetSquared);
            const float distance = static_cast<float>(std::sqrt(static_cast<double>(squaredDistance)));
            const float wave = ppc_fnmsubs(kFlagSpeed, phase, distance);     // distance - 0.6 * phase
            const float turnedWave = kWaveTurn * wave;                       // f6
            const float quarterWave = kHalfPiLow * wave;                     // f2
            const float width = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(columns) - 4u)) * step;
            const float fraction = columnCoord / width;
            // fcmpo and bge: the clamp wins only when it is below the fraction (a NaN fraction stays).
            const float clamp = gDynamicFlagClamp;
            const float stretch = clamp < fraction ? clamp : fraction;       // f5
            const float scale = gTPGlobals.mSinTableScale;
            const float shiftedTurned = kHalfPi + turnedWave;
            const float shiftedQuarter = kHalfPi + quarterWave;
            const float scaledShiftedTurned = shiftedTurned * scale;
            const float scaledShiftedQuarter = shiftedQuarter * scale;
            const int32_t indexTurned = ppc_fctiwz(scaledShiftedTurned);
            const int32_t indexQuarter = ppc_fctiwz(scaledShiftedQuarter);
            const float scaledTurned = turnedWave * scale;
            const int32_t indexPlain = ppc_fctiwz(scaledTurned);
            const uint32_t mask = gTPGlobals.mSinTableMask;
            const float twiceStretch = two * stretch;
            const float heightScale = kRipple * twiceStretch;                // f1
            const float widthScale = kRipple * stretch;                      // f2
            const float tableQuarter = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(indexQuarter)];    // f0
            const float tableTurned = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(indexTurned)];      // f3
            const float lift = tableQuarter * heightScale;
            const float depth = tableTurned * widthScale;                    // f4
            const float negStretch = -stretch;                               // f2
            position.x = columnCoord;
            const float lifted = rowCoord + lift;
            const float droop = kDroop * columnCoord;
            position.y = ppc_fnmsubs(droop, columnCoord, lifted);            // lifted - droop * x
            position.z = zero + depth;
            const float tablePlain = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(indexPlain)];
            const float product = negStretch * tablePlain;                   // f5
            const double zeroSquared = static_cast<double>(zero) * static_cast<double>(zero);
            const double scaledProduct = static_cast<double>(product) * static_cast<double>(one);
            normal.x = static_cast<float>(ppc_fmsub(static_cast<double>(zero), static_cast<double>(product), scaledProduct));
            normal.y = static_cast<float>(ppc_fmsub(static_cast<double>(product), static_cast<double>(zero), scaledProduct));
            normal.z = static_cast<float>(ppc_fmsub(static_cast<double>(one), static_cast<double>(one), zeroSquared));
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
        rowCoord = rowCoord - step;
    }
}
