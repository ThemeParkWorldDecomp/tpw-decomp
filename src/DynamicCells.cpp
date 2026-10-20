// The dynamic cell meshes in the frame (analyser.cpp of the main app; unit w120): DC_TickDynamicCellMeshes (0x100dfd88, 1,012 bytes),
// CDynamicMesh::DrawAll (0x100e1a04, 1,248 bytes) and CDynamicMesh::DC_CreatePolyFace (0x100e2778, 232 bytes).
//
// The tick runs every park frame (GAMESYSTEM_Tick). It scrolls the texture coordinate table the squares use, animates the water and the
// flags, asks the flag code for the eight flags, sorts the list of dynamic faces by texture, lets the three meshes rebuild themselves from
// the list (DrawAll), empties the list, and, while the clock is not paused, advances the water and flag phases and the cursor blink.
// The list is gDynamicFaces (0xa0-byte records) with gDynamicFaceKeys naming them: key = (record index << 5) | (texture & 0x1f).
// The producers (DC_AddSquareFaceToList, DC_AddPolyFaceToList, DC_DrawWater, DC_DrawFlag) are not translated yet, so the list is empty
// and DrawAll sets the three meshes' counts to 0.
#include "DynamicMesh.h"

#include "Clock.h"
#include "Debug.h"
#include "TPCamera.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "ppc_math.h"

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
