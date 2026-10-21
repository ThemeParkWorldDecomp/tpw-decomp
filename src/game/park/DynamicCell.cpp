// DynamicCell.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Clock.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "Game.h"
#include "GameSys.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "platform/opengl.h"
#include "ppc_math.h"
#include "rsse.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "World.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

// The script system's start and the dynamic cell meshes' set-up and teardown (main application, analyser.cpp range; unit w111).

int32_t gDynamicCellF9BD4;
uint32_t gDynamicCellsReady;
uint32_t gDynamicCellRandom[110];
int32_t gDynamicCellF9D94;
float gDynamicCellF9D98;
float gDynamicCellF9D9C;
float gDynamicCellF9DA0;
int32_t gDynamicCellFBDBC;
uint8_t gDynamicCellFlag = 1;     // initialised data
CDynamicMesh* gBlueprintMesh;
CDynamicMesh* gWaterMesh;
CDynamicMesh* gFlagsMesh;

namespace {

// The box every dynamic mesh is made in: 0 to 1024 across, from -10 to -200 in depth (the constants at 0x10265088 to 0x1026507c).
CDynamicMesh* MakeMesh(uint16_t sizeA, const char* name, DYN_Type type, uint32_t* ok) {
    return new (std::nothrow) CDynamicMesh(sizeA, 0x14, ok, name, type, 0.0f, -10.0f, -200.0f, 1024.0f, 1024.0f, 1024.0f);
}

}  // namespace

// 0x100df8fc (572 bytes). The constructor's report (the first word of a four word buffer on the stack) decides whether the
// textures are given back: the first mesh's failure frees them, a later mesh's only if no mesh failed before. The buffer is not
// cleared between the meshes, so a mesh whose memory could not be had (the constructor does not run) is judged by the report
// of the one before it; for the first mesh the Mac reads an unset stack word (the host reads 0).
void DC_SetupDynamicCellMeshes() {
    gDynamicCellF9BD4 = 0;
    if (DMLoadTextures(&gDynamicCellTextures) != 0) {
        gDynamicCellFBDBC = 0;
        gDynamicCellF9DA0 = 0.0f;
        gDynamicCellF9D9C = 0.0f;
        gDynamicCellF9D98 = 0.0f;
        gDynamicCellFlag = 1;
        gDynamicCellF9D94 = 0;
        bool noFailure = true;
        for (int32_t i = 0; i < 0x6e; ++i) gDynamicCellRandom[i] = static_cast<uint32_t>(msl_rand() % 3 + 0xf);
        uint32_t ok[4] = {};
        gBlueprintMesh = MakeMesh(0x600, "BlueprintMesh", kDynBlueprint, ok);
        if (ok[0] == 0) {
            noFailure = false;
            DMFreeTextures(&gDynamicCellTextures);
        }
        gWaterMesh = MakeMesh(0x86, "WaterMesh", kDynWater, ok);
        if (noFailure && ok[0] == 0) {
            noFailure = false;
            DMFreeTextures(&gDynamicCellTextures);
        }
        gFlagsMesh = MakeMesh(0x180, "FlagsMesh", kDynFlags, ok);
        if (noFailure && ok[0] == 0) DMFreeTextures(&gDynamicCellTextures);
        gDynamicCellsReady = 1;
    }
}

// The dynamic face list and the water (analyser.cpp of the main app; unit w127): sort (0x100dfb68, 512 bytes),
// DC_ResetSquareFaceListPtr (0x100e0260, 16), DC_AddSquareFaceToList (0x100e02a0, 268), DC_AddPolyFaceToList (0x100e03f0, 256),
// HDZ_RotatePointZ (0x100e0544, 148), DC_DrawWater (0x100e060c, 580) and DC_AnimateWater (0x100e10f4, 560).
//
// The face list is gDynamicFaces (0xa0-byte records, src/DynamicCells.cpp) with gDynamicFaceKeys naming them; the two adders append to it
// and the tick (DC_TickDynamicCellMeshes) clears it, sorts the keys by texture with `sort` and draws it with CDynamicMesh::DrawAll.
// DC_AnimateWater fills two 16 x 19 grids of points (the surface normals and the positions of the water) and DC_DrawWater turns them into
// one poly face per cell, 10 x 11 and 4 x 6 cells of the grids with an offset (the tick's two calls), each with a texture from
// gDynamicCellRandom. Every sine of the area is the engine's table: gTPGlobals.mSinTable[mask & (int)(angle * scale)].

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

// The dynamic cell meshes in the frame (analyser.cpp of the main app; unit w120): DC_TickDynamicCellMeshes (0x100dfd88, 1,012 bytes),
// CDynamicMesh::DrawAll (0x100e1a04, 1,248 bytes) and CDynamicMesh::DC_CreatePolyFace (0x100e2778, 232 bytes).
//
// The tick runs every park frame (GAMESYSTEM_Tick). It scrolls the texture coordinate table the squares use, animates the water and the
// flags, asks the flag code for the eight flags, sorts the list of dynamic faces by texture, lets the three meshes rebuild themselves from
// the list (DrawAll), empties the list, and, while the clock is not paused, advances the water and flag phases and the cursor blink.
// The list is gDynamicFaces (0xa0-byte records) with gDynamicFaceKeys naming them: key = (record index << 5) | (texture & 0x1f).
// The producers (DC_AddSquareFaceToList, DC_AddPolyFaceToList, DC_DrawWater, DC_DrawFlag) are not translated yet, so the list is empty
// and DrawAll sets the three meshes' counts to 0.

// ---- globals (BSS on the Mac, so zero at start; the flag spots are initialised data of the image)
sDynamicFace gDynamicFaces[kDynamicFaceMax];   // 0x102fbdc0
uint32_t gDynamicFaceKeys[kDynamicFaceMax];    // 0x102f9da4
sDynamicFlagSpot gDynamicFlagSpots[8] = {      // 0x102a0320 (mF00 is 0xffffffff in the image)
    {-1, 411.25f, 22.0f, 38.75f}, {-1, 411.25f, 22.0f, 50.75f}, {-1, 451.25f, 22.0f, 38.75f}, {-1, 451.25f, 22.0f, 50.75f},
    {-1, 511.25f, 22.0f, 38.75f}, {-1, 511.25f, 22.0f, 50.75f}, {-1, 551.25f, 22.0f, 38.75f}, {-1, 551.25f, 22.0f, 50.75f},
};

// 0x100dfd88 (1,012 bytes). The listing reads every input before it stores anything, and re-reads the two scroll lags after the rotate call:
// the old lags feed the fused subtractions, the lags as the call left them get the fused additions.
void DC_TickDynamicCellMeshes() {
    // The map origin (two ints, z then x) as floats, and the old lags (x first): the lag moves half way to ten times the origin.
    const float originZ = static_cast<float>(gLLCCDB50);
    const float originX = static_cast<float>(gLLCCDB54);
    const float oldLagX = gDynamicCellF9BC4;
    const float oldLagZ = gDynamicCellF9BC0;
    const float lagX = ppc_fmsubs(10.0f, originX, oldLagX);
    const float lagZ = ppc_fmsubs(10.0f, originZ, oldLagZ);

    // The texture coordinate rows (u, v; the third float of a row is never written): the unit square's corners, the centre, and four
    // corners of a 0.8 square that the camera angle turns.
    float* uv = &gDynamicCellUV[0][0];
    uv[0x00 / 4] = 0.0f;
    uv[0x04 / 4] = 0.0f;
    uv[0x0c / 4] = 1.0f;
    uv[0x10 / 4] = 0.0f;
    uv[0x18 / 4] = 0.0f;
    uv[0x1c / 4] = 1.0f;
    uv[0x24 / 4] = 1.0f;
    uv[0x28 / 4] = 1.0f;
    uv[0x30 / 4] = 0.5f;
    uv[0x34 / 4] = 0.5f;
    uv[0x3c / 4] = -0.4f;
    uv[0x40 / 4] = -0.4f;
    uv[0x48 / 4] = 0.4f;
    uv[0x4c / 4] = -0.4f;
    uv[0x54 / 4] = -0.4f;
    uv[0x58 / 4] = 0.4f;
    uv[0x60 / 4] = 0.4f;
    uv[0x64 / 4] = 0.4f;
    HDZ_RotatePointZ(reinterpret_cast<s3DPoint*>(&gDynamicCellUV[5][0]), gCameraF5BF4);
    // The rotated corners are then moved by 0.5 (the rotation is about the origin); row 9 is not rotated.
    uv[0x3c / 4] = uv[0x3c / 4] + 0.5f;
    uv[0x40 / 4] = uv[0x40 / 4] + 0.5f;
    uv[0x48 / 4] = uv[0x48 / 4] + 0.5f;
    uv[0x4c / 4] = uv[0x4c / 4] + 0.5f;
    uv[0x54 / 4] = uv[0x54 / 4] + 0.5f;
    uv[0x58 / 4] = uv[0x58 / 4] + 0.5f;
    uv[0x60 / 4] = uv[0x60 / 4] + 0.5f;
    uv[0x64 / 4] = uv[0x64 / 4] + 0.5f;
    uv[0x6c / 4] = 0.5f;
    uv[0x70 / 4] = 0.5f;
    gDynamicCellF9BC4 = ppc_fmadds(lagX, 0.5f, gDynamicCellF9BC4);
    gDynamicCellF9BC0 = ppc_fmadds(lagZ, 0.5f, gDynamicCellF9BC0);

    // The water: its phase is animated, then two grids of faces are drawn from the water's start position (read again for each call).
    DC_AnimateWater(0x10, 0x13, 20.0f, gDynamicCellF9D98);
    DC_DrawWater(gWaterStartPos.x, gWaterStartPos.y, gWaterStartPos.z, 10, 0xb, 0, 0, 0x10, 0x13);
    DC_DrawWater(gWaterStartPos.x, gWaterStartPos.y, gWaterStartPos.z, 4, 6, 10, 0, 0x10, 0x13);

    // The eight flags, blowing in the weather's wind (degrees at +0x5c of the weather thing, which CWeather's constructor leaves at 90.0f).
    // They alternate between texture 0x12 and 0x13.
    DC_AnimateFlag(7, 5, 1.8f, gDynamicCellF9D9C);
    int32_t texture = 0x12;
    for (int32_t i = 0; i < 8; ++i) {
        const CWeather* weather = gGameWorld->GetWeather();
        const float wind = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(weather) + 0x5c);   // (CWeather has no members yet)
        const sDynamicFlagSpot& spot = gDynamicFlagSpots[i];
        DC_DrawFlag(spot.mX, spot.mY, spot.mZ, 0.5f, 6, 4, texture, 0.017453299835324287f * wind);
        if (texture == 0x12) {
            texture = 0x13;
        } else if (texture == 0x13) {
            texture = 0x12;
        }
    }

    // Group the list by texture, rebuild the three meshes from it and empty the list. (The sort takes the array's address less one word:
    // it counts from 1.)
    sort(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(gDynamicFaceKeys) - sizeof(uint32_t)), static_cast<uint32_t>(gDynamicCellFBDBC));
    gBlueprintMesh->DrawAll();
    gWaterMesh->DrawAll();
    gFlagsMesh->DrawAll();
    DC_ResetSquareFaceListPtr();

    // While the game is running: the water and flag phases advance with the game speed, and a counter in whole speed units blinks the
    // cursor squares (on for 7, off for 2).
    if (gClock.GetPauseFlag() == 0) {
        gDynamicCellF9DA0 = gDynamicCellF9DA0 + 0.1f;
        CVariableTime& speed = gClock.mTime.mPausableTime.mVariableTime;
        const float waterRate = static_cast<float>(speed.GetTimeRate());
        gDynamicCellF9D98 = ppc_fmadds(0.4f, waterRate, gDynamicCellF9D98);
        const float flagRate = static_cast<float>(speed.GetTimeRate());
        gDynamicCellF9D9C = ppc_fmadds(0.4f, flagRate, gDynamicCellF9D9C);
        const int32_t step = ppc_fctiwz(static_cast<double>(static_cast<float>(speed.GetTimeRate())));
        gDynamicCellF9D94 = static_cast<int32_t>(static_cast<uint32_t>(gDynamicCellF9D94) + static_cast<uint32_t>(step));
        if (gDynamicCellFlag != 0) {
            if (gDynamicCellF9D94 > 6) {
                gDynamicCellFlag = 0;
                gDynamicCellF9D94 = 0;
            }
        } else if (gDynamicCellF9D94 > 1) {
            gDynamicCellFlag = 1;
            gDynamicCellF9D94 = 0;
        }
    }
}

// 0x100e01ac (132 bytes): the three destructors are called with the deleting flag (a NULL mesh does nothing).
void DC_FreeDynamicCellMeshes() {
    if (gDynamicCellsReady != 0) {
        delete gBlueprintMesh;
        delete gWaterMesh;
        delete gFlagsMesh;
        DMFreeTextures(&gDynamicCellTextures);
    }
    gDynamicCellsReady = 0;
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

// The flags of the park (analyser.cpp of the main app; unit w128): DC_DrawFlag (0x100e087c, 2,124 bytes) and DC_AnimateFlag
// (0x100e1350, 664 bytes).
//
// The tick (DC_TickDynamicCellMeshes, src/DynamicCells.cpp) calls DC_AnimateFlag(7, 5, 1.8f, phase) once a frame and then DC_DrawFlag once per
// flag spot. DC_AnimateFlag fills the same two 16 x 19 grids as the water (src/DynamicFaces.cpp) for a 7 x 5 cloth: the positions grid with
// the points of the waving flag (x across, y down, z the ripple) and the normals grid with their normals. DC_DrawFlag turns the cloth into
// two poly faces (type kDynFlags, the texture the tick names) per cell of a columns x rows block, a back face with the normals negated and a
// front face with the normals as they are, turned about the y axis by the wind's angle and moved to the spot. Every sine of the area is the
// engine's table: gTPGlobals.mSinTable[mask & (int)(angle * scale)].

// 0x102a03a0 (TOC 0x10262c90): the largest fraction of the flag's width that DC_AnimateFlag lets the ripple grow with. Initialised data, 1.0f.
float gDynamicFlagClamp = 1.0f;

namespace {

constexpr float kHalfPiLow = 1.5707963f;    // 0x3fc90fda (0x10265030): one unit less in the last place; DC_AnimateFlag multiplies by it
constexpr float kFlagSpeed = 0.6f;          // 0x3f19999a (0x10265034)
constexpr float kRipple = 0.3f;             // 0x3e99999a (0x10265028)
constexpr float kDroop = 0.02f;             // 0x3ca3d70a (0x10265024)
constexpr float kTextureHeight = 0.55f;     // 0x3f0ccccd (0x10265048): the v range of texture 0x13

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

// The dynamic cell meshes (main application; unit w112b). The Mac files are analyser.cpp for the class (CDynamicMesh: constructor
// 0x100e1610, destructor 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10, DC_CreateSquareFace 0x100e210c), TPAdvisor.cpp
// for DMCreateMesh 0x100301c0 and the two DMEnableMeshRender 0x1003088c and 0x10030928, rsse.cpp for CorrectDTexturePaths 0x100b8710 and
// LLCellSystem.cpp for LLCC_SystemGetMode 0x100914f4.
//
// A mesh is N squares of four vertices and two faces each, drawn cell by cell by DrawAll (untranslated, the next unit). DMCreateMesh lays the
// whole mesh image out in one block of the engine allocator in the mesh file's 32-bit layout (Ptr32 fields; the block comes from the arena);
// the CDynamicMesh itself, its texture set, entry and mesh set are native records that point into the block.

// ---- globals (all zero at start, like the Mac's bss)
float gDynamicCellUV[10][3];
float gDynamicCellF9BC0;
float gDynamicCellF9BC4;
char gDynamicCellTexturePath[0x400];
int32_t gLLCCDB50;
int32_t gLLCCDB54;

namespace {

// The strings of the Mac's string area at 0x1023ff4d the constructor uses.
const char kTexturesFolder[] = "Data:Generic:Dynamic:Textures";   // 0x1024001f
const char kSmallTexturesFolder[] = "Data:Generic:Dynamic:STexture";   // 0x102400a3
const char kLevelsFolder[] = "Data:Levels:";                      // 0x102400c1
const char kSmallLevelFolder[] = ":Dynamic:STexture";             // 0x102400ce
const char kLevelFolder[] = ":Dynamic:Textures";                  // 0x102400e0

}  // namespace

int32_t LLCC_SystemGetMode();

int32_t DMEnableMeshRender(tag_sDMeshSet* set);

int32_t DMEnableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to);

int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags);

void CorrectDTexturePaths(sDTextureSet* textures, tag_sDMeshSet* set);

// ---- analyser.cpp: CDynamicMesh

// 0x100e1610 (716 bytes). Bytes the constructor leaves as the heap had them: the entry's +2 and +0xe and the set's +6.
CDynamicMesh::CDynamicMesh(uint16_t sizeA, uint16_t sizeB, uint32_t* ok, const char* name, DYN_Type type, float minX, float minY,
                           float minZ, float maxX, float maxY, float maxZ) {
    *ok = 1;
    mType = type;
    mTextures.mFlags = 0;
    mTextures.mCount = sizeB;
    mTextures.mFolder = UseSmallTextures() ? kSmallTexturesFolder : kTexturesFolder;
    std::strcpy(gDynamicCellTexturePath, kLevelsFolder);
    std::strcat(gDynamicCellTexturePath, gGame.GetCurrentThemeName().mData);
    std::strcat(gDynamicCellTexturePath, UseSmallTextures() ? kSmallLevelFolder : kLevelFolder);
    mTextures.mSharedFolder = gDynamicCellTexturePath;
    mTextures.mEntries = gDynamicCellTextureEntries;

    mMeshSet.mF00 = 3;
    mMeshSet.mCount = 1;
    mMeshSet.mEntries = &mItem;
    mMeshSet.mMatrix = nullptr;
    mMeshSet.mVertexSets = nullptr;
    mMeshSet.mPoints = nullptr;
    mMeshSet.mUVSets = nullptr;
    mMeshSet.mFaces = nullptr;
    mMeshSet.mBlock20 = nullptr;
    mMeshSet.mMesh = nullptr;
    mMeshSet.mTextureCount = 0;
    mMeshSet.mVertexCount = 0;
    mMeshSet.mFaceSetCount = 0;
    mMeshSet.mFaceCount = 0;
    mMeshSet.mUVCount = 0;
    mMeshSet.mPointCount = 0;
    mMeshSet.mAllocation = nullptr;
    mMeshSet.mF38 = 0;
    mMeshSet.mShared = nullptr;

    mItem.mFlags = 3;
    mItem.mFaceCount = static_cast<uint16_t>(sizeA << 1);
    mItem.mVertexCount = static_cast<uint16_t>(sizeA << 2);
    mItem.mUVCount = mItem.mVertexCount;
    mItem.mPointCount = 0;
    mItem.mFaceSetCount = sizeB;
    mMaxSquares = sizeA;
    SetBoundingBox(minX, minY, minZ, maxX, maxY, maxZ);

    *ok = static_cast<uint32_t>(DMCreateMesh(&mMeshSet, &mTextures, name, 3));
    if (*ok == 1) {
        SetMatrixIdentity(mItem.mMatrix);
        mVertexCount = 0;
        mFaceCount = 0;
        mFaceCursor = mMeshSet.mFaces;
        for (int32_t i = 0; i < static_cast<int32_t>(sizeA); ++i) DC_CreateSquareFace(0.0f, 0.0f, 0.0f, 0, 0xff, -1, 1.0f, 0, 0, 0);
        for (int32_t i = 0; i < static_cast<int32_t>(sizeB); ++i) {
            sFaceSet* faceSet = mItem.mFaceSets + i;
            faceSet->mTexture = mTextures.mTextures + i;
            faceSet->mFirstFace = 0;
            faceSet->mFaceCount = 0;
        }
        DMEnableMeshRender(&mMeshSet);
    }
}

// 0x100e191c (104 bytes). The Mac's NULL test and its operator delete (for a positive flag) are C++'s `delete`.
CDynamicMesh::~CDynamicMesh() {
    DMDisableMeshRender(&mMeshSet);
    DMFreeMesh(&mMeshSet);
}

// 0x100e19b0 (28 bytes).
void CDynamicMesh::SetBoundingBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    mItem.mBox[0] = minX;
    mItem.mBox[1] = minY;
    mItem.mBox[2] = minZ;
    mItem.mBox[3] = maxX;
    mItem.mBox[4] = maxY;
    mItem.mBox[5] = maxZ;
}

namespace {

// The box accumulators of DrawAll are compared with fcmpo and bge, so a NaN takes the accumulator: the upper corner takes the new value only
// when the accumulator is below it, the lower corner only when the new value is below the accumulator.
float SelectHigh(float accumulator, float value) { return accumulator < value ? value : accumulator; }
float SelectLow(float accumulator, float value) { return value < accumulator ? value : accumulator; }

}  // namespace

// 0x100e1a04 (1,248 bytes). The six accumulators of the bounding box are initialised (-1024 and 1024) and never updated, so the box stored
// is the last drawn face's first corner plus and minus 10, each clamped to +-1024 (a NaN corner gives the clamp). A face of this mesh's
// type is drawn only while fewer than mMaxSquares have been; a refused one still starts its texture's run of faces.
void CDynamicMesh::DrawAll() {
    float accumulatorHigh[3] = {-1024.0f, -1024.0f, -1024.0f};
    float accumulatorLow[3] = {1024.0f, 1024.0f, 1024.0f};
    mVertexCount = 0;
    mFaceCount = 0;
    mFaceCursor = mMeshSet.mFaces;
    for (int32_t j = 0; j < 20; ++j) {
        mItem.mFaceSets[j].mFaceCount = 0;
        mItem.mFaceSets[j].mFirstFace = 0;
        mItem.mFaceSets[j].mF08 = 0;
        mItem.mFaceSets[j].mF0A = 0;
    }
    bool found = false;
    int32_t current = -1;
    int32_t drawn = 0;
    for (int32_t i = 0; i < gDynamicCellFBDBC; ++i) {
        sDynamicFace& face = gDynamicFaces[gDynamicFaceKeys[i] >> 5];
        if (mType != face.mType) {
            // The list is grouped by type: the first face of another type after one of ours ends the walk.
            if (found) break;
            continue;
        }
        found = true;
        const int32_t texture = face.mTexture;
        if (current != texture) {
            mItem.mFaceSets[texture].mFirstFace = static_cast<uint16_t>(mFaceCount);
            mItem.mFaceSets[texture].mF08 = static_cast<uint16_t>(mVertexCount);
            current = texture;
        }
        if (face.mIsSquare == 0) {
            if (drawn >= mMaxSquares) {
                CTRACE(6, "Max faces exceeded in Drawall\n");
                continue;
            }
            DC_CreatePolyFace(face.mPositions, face.mTexture, face.mUVs, face.mPoints);
        } else {
            if (drawn >= mMaxSquares) {
                CTRACE(6, "Max faces exceeded in Drawall\n");
                continue;
            }
            DC_CreateSquareFace(face.mX, face.mY, face.mZ, face.mSide, static_cast<uint8_t>(face.mRotation), face.mTexture, face.mF14, face.mLarge,
                                face.mF1C, face.mKeepPosition);
        }
        // The box (z, y, x order; every value goes through a single-precision temporary).
        const float highZ = SelectHigh(accumulatorHigh[0], 10.0f + face.mZ);
        const float highY = SelectHigh(accumulatorHigh[1], 10.0f + face.mY);
        const float highX = SelectHigh(accumulatorHigh[2], 10.0f + face.mX);
        const float lowZ = SelectLow(accumulatorLow[0], face.mZ - 10.0f);
        const float lowY = SelectLow(accumulatorLow[1], face.mY - 10.0f);
        const float lowX = SelectLow(accumulatorLow[2], face.mX - 10.0f);
        drawn = drawn + 1;
        mItem.mBox[0] = lowX;
        mItem.mBox[1] = lowY;
        mItem.mBox[2] = lowZ;
        mItem.mBox[3] = highX;
        mItem.mBox[4] = highY;
        mItem.mBox[5] = highZ;
        mItem.mFaceSets[current].mFaceCount = static_cast<uint16_t>(mFaceCount - mItem.mFaceSets[current].mFirstFace);
        mItem.mFaceSets[current].mF0A = static_cast<uint16_t>(mVertexCount - 1);
    }
    mItem.mFaceCount = static_cast<uint16_t>(static_cast<uint32_t>(drawn) << 1);
    mItem.mVertexCount = static_cast<uint16_t>(static_cast<uint32_t>(drawn) << 2);
    mItem.mUVCount = mItem.mVertexCount;
}

// 0x100e1f10 (444 bytes). With followTerrain the vertex's height is the terrain's plus a wave: the sine table at (z + (gDynamicCellF9DA0 + x))
// times the table's scale (the listing adds the phase to x first), halved after adding 1 for the point's y, and added to the height as it
// is. Without it the point is the one given (or straight up) and the height is y unchanged. The positions are stored four to a group (x, y and
// z arrays of four), the texture coordinates likewise.
void CDynamicMesh::DC_SetVertex(float x, float y, float z, float u, float v, int32_t followTerrain, s3DPoint* point) {
    s3DPoint* entry = mMeshSet.mPoints + mVertexCount;
    if (followTerrain != 0) {
        const float height = DetermineHFieldHeightQuick(x, z);
        y = y + height;
        const float phase = gDynamicCellF9DA0 + x;
        const float place = z + phase;
        const float scaled = gTPGlobals.mSinTableScale * place;
        const uint32_t index = gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)));
        const float wave = gTPGlobals.mSinTable[index];
        entry->x = 0.0f;
        entry->y = (1.0f + wave) * 0.5f;
        entry->z = 0.0f;
        y = y + wave;
    } else if (point == nullptr) {
        entry->x = 0.0f;
        entry->y = 1.0f;
        entry->z = 0.0f;
    } else {
        entry->x = point->x;
        entry->y = point->y;
        entry->z = point->z;
    }
    float* positions = reinterpret_cast<float*>(mMeshSet.mVertexSets + (mVertexCount >> 2)) + (mVertexCount & 3);
    positions[0] = x;
    positions[4] = y;
    positions[8] = z;
    float* uvs = reinterpret_cast<float*>(mMeshSet.mUVSets + (mVertexCount >> 2)) + (mVertexCount & 3);
    uvs[0] = u;
    uvs[4] = v;
    mVertexCount = mVertexCount + 1;
}

// 0x100e210c (1,576 bytes). A face's corner order (which table entry each vertex takes) comes from `rotation`, the geometry from `side`; the
// map's scroll lag is taken off the position for textures below 13 while the cell system is in mode 4, 0xe or 0x13. Both faces use the vertex
// counter as it was at entry.
void CDynamicMesh::DC_CreateSquareFace(float x, float y, float z, uint8_t side, uint8_t rotation, int32_t texture, float, int32_t large,
                                       uint8_t, int32_t keepPosition) {
    const uint32_t first = mVertexCount;
    const float size = large != 0 ? 20.0f : 10.0f;
    if (keepPosition == 0 && gDynamicCellF9BD4 == 0 && texture < 0xd &&
        (LLCC_SystemGetMode() == 4 || LLCC_SystemGetMode() == 0xe || LLCC_SystemGetMode() == 0x13)) {
        const float originX = static_cast<float>(gLLCCDB54);
        const float originZ = static_cast<float>(gLLCCDB50);
        x = ppc_fnmsubs(10.0f, originX, x);
        z = ppc_fnmsubs(10.0f, originZ, z);
        x = x + gDynamicCellF9BC4;
        z = z + gDynamicCellF9BC0;
    }
    const int32_t followTerrain = (texture == -1 || texture >= 0xf) ? 0 : 1;
    int32_t base = 0;
    if (texture < 0xf && texture >= 8) base = 5;
    int32_t a, b, c, d;
    if (rotation == 0x10) {
        a = base + 3;
        b = base + 2;
        c = base + 1;
        d = base;
    } else if (rotation == 4) {
        a = base + 2;
        b = base;
        c = base + 3;
        d = base + 1;
    } else if (rotation == 0x40) {
        a = base + 1;
        b = base + 3;
        c = base;
        d = base + 2;
    } else {
        a = base;
        b = base + 1;
        c = base + 2;
        d = base + 3;
    }
    const float(*uv)[3] = gDynamicCellUV;
    if (side == 4) {
        const float right = x + size;
        DC_SetVertex(right, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(right, y, z + size, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(right, top, z, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(right, top, z + size, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 1) {
        DC_SetVertex(x, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x, top, z, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x + size, top, z, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0) {
        DC_SetVertex(x, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        DC_SetVertex(x, y, z + size, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z + size, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0x40) {
        DC_SetVertex(x, y, z + size, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x, top, z + size, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x, top, z, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0x10) {
        const float back = z + size;
        DC_SetVertex(x + size, y, back, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x, y, back, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x + size, top, back, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x, top, back, uv[d][0], uv[d][1], followTerrain, nullptr);
    }
    // Both faces, whatever the side (an unknown side made no vertices, so its faces name the next square's).
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first + 1);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first + 3);
    mFaceCursor += 1;
    mFaceCount += 1;
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first + 2);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first + 3);
    mFaceCursor += 1;
    mFaceCount += 1;
}

// 0x100e2778 (232 bytes): four vertices, then two faces over them: (first + 2, first + 1, first) and (first + 1, first + 2, first + 3). The
// first vertex index is read before the vertices are made; the texture is not read.
void CDynamicMesh::DC_CreatePolyFace(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points) {
    const uint32_t first = mVertexCount;
    for (int32_t k = 0; k < 4; ++k) {
        DC_SetVertex(positions[k].x, positions[k].y, positions[k].z, uvs[k].x, uvs[k].y, 0, &points[k]);
    }
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first + 2);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first + 1);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first);
    mFaceCursor = mFaceCursor + 1;
    mFaceCount = mFaceCount + 1;
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first + 1);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first + 2);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first + 3);
    mFaceCursor = mFaceCursor + 1;
    mFaceCount = mFaceCount + 1;
}

int32_t Rand();   // 0x10239148 rand

// 0x102a03a4: four RGBA colours of doubles; the original's initial data is not known here (zero).
double gWhiteOutColors[16];

// 0x100e28b4 (368 bytes): fills the screen with 4-quad-sized random tiles (quads of 0.00625 x 0.008333 over [-1, 1]), each tile
// coloured by one of the four colours picked with rand() & 3. The colour table is copied to the stack first.
void STATIC_WhiteOut() {
    double colors[16];
    for (int i = 0; i < 16; ++i) colors[i] = gWhiteOutColors[i];
    glPushAttrib(0x2000);
    glDisable(0xb71);
    glDisable(0xde1);
    glDisable(0xbc0);
    glDisable(0xbe2);
    glDisable(0xb60);
    glBegin(7);
    double x = -1.0;
    while (x <= 1.0) {
        double y = -1.0;
        while (y <= 1.0) {
            const uint32_t r = static_cast<uint32_t>(Rand());
            glColor4dv(&colors[(r & 3) * 4]);
            const double x2 = x + 0.00625;
            const double y2 = y + 0.008333333333333333;
            glVertex2d(x, y);
            glVertex2d(x2, y);
            glVertex2d(x2, y2);
            glVertex2d(x, y2);
            y = y2;
        }
        x = x + 0.00625;
    }
    glEnd();
    glPopAttrib();
}
