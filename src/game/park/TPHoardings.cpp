// TPHoardings.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "keytable_actions.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "tb_matrix.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

// 0x1005f8c4 (380 bytes): the angle from (halfWidth, halfHeight) to the middle of one side (0 to 3) of cell (x, y),
// atan2 in double, plus pi and a quarter pi in single, wrapped by fmod at 2 pi. Sides outside 0..3 leave the registers
// unset in the original; here the point is (0, 0).
float HoardingCentreAngle(int32_t x, int32_t y, int32_t side, float halfWidth, float halfHeight) {
    const float fx = static_cast<float>(static_cast<double>(x));
    const float fy = static_cast<float>(static_cast<double>(y));
    float px = 0.0f;
    float py = 0.0f;
    switch (side) {
        case 0: px = 0.5f + fx; py = 1.0f + fy; break;
        case 1: px = 1.0f + fx; py = 0.5f + fy; break;
        case 2: px = 0.5f + fx; py = fy; break;
        case 3: px = fx; py = 0.5f + fy; break;
        default: break;
    }
    const float dy = py - halfHeight;
    const float dx = px - halfWidth;
    const float a = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));
    const float s = 0.7853982f + (3.1415927f + a);
    return static_cast<float>(std::fmod(static_cast<double>(s), 6.2831854820251465));
}

// GenerateHoardingMeshes (main app 0x1005fa70, 4100 bytes; TPRide.cpp in the inventory), unit v5625: the dynamic mesh of a ride's
// hoardings, one quad per marked side of the ride's shape, the quads numbered in the order of their angle round the ride's centre.

const char* gHoardingTexturePathFormat = "data:levels:%s:MiscMesh:textures";   // 0x1029cf14
void* gHoardingSortScratch[1600];                                              // 0x102c2150
void* gHoardingSortSource[1600];                                               // 0x102c3a50
int32_t gHoardingSortOrder[1600];                                              // 0x102c5350

namespace {

// The side bits of a shape cell's flag byte, in side order.
const uint32_t kSideBits[4] = {1, 4, 0x10, 0x40};

const float kWallHeight = 10.0f;   // also the cell size
const float kNoAngle = 3.4028234663852886e38f;

// The quad of one side: lanes 0 and 1 at the top (a, then b), 2 and 3 at the bottom; the four points (nx, 0, nz) and the fifth
// point, the reverse.
void WriteSide(tag_sDMeshSet* set, uint32_t index, uint32_t quads, float ax, float az, float bx, float bz, float nx, float nz) {
    sVertexPosnSet& v = set->mVertexSets[index & 0x3fffffffu];
    v.x[0] = ax;
    v.y[0] = kWallHeight;
    v.z[0] = az;
    v.x[1] = bx;
    v.y[1] = kWallHeight;
    v.z[1] = bz;
    v.x[2] = ax;
    v.y[2] = 0.0f;
    v.z[2] = az;
    v.x[3] = bx;
    v.y[3] = 0.0f;
    v.z[3] = bz;
    for (uint32_t k = 0; k < 4; ++k) {
        s3DPoint& p = set->mPoints[static_cast<uint32_t>(index * 4 + k)];
        p.x = nx;
        p.y = 0.0f;
        p.z = nz;
    }
    s3DPoint& back = set->mPoints[static_cast<uint32_t>(index + quads)];
    back.x = 0.0f - nx;   // +0.0 stays +0.0, as the Mac's constants
    back.y = 0.0f;
    back.z = 0.0f - nz;
}

void WriteCorners(uint16_t* block, uint32_t index, int32_t a, int32_t b, int32_t c, int32_t d) {
    uint16_t* corner = block + static_cast<uint32_t>(index * 4);
    corner[0] = static_cast<uint16_t>(a);
    corner[1] = static_cast<uint16_t>(b);
    corner[2] = static_cast<uint16_t>(c);
    corner[3] = static_cast<uint16_t>(d);
}

}  // namespace

// 0x1005fa70: nothing when the set's mesh has fewer than two heads (the u16 at mesh + 0xc). Otherwise every side of the shape's cells
// (bits 1, 4, 0x10, 0x40 of a cell's flag) gets an angle from HoardingCentreAngle; FloatSortByArray orders them and each side's quad
// takes its sorted index. The mesh set (one entry, 4 vertices, 5 points, 1 UV set and 4 faces per quad) goes through DMCreateMesh with
// the textures of "data:levels:<theme>:MiscMesh:textures"; with a root node the quads are filled (DetermineInnerBaseInsets moves the
// corners in, unless the shape is one cell) and the set and the geometry block go to the ride set (+0xb4, +0xb0), its flag 2 set.
// Kept from the original: the texture set's folder is left pointing at this function's buffer; the set, the entry and the corner block
// leak when there is no root node. The shape is at most 20 x 20 (larger overruns the tables, as on the Mac).
void GenerateHoardingMeshes(const char* theme, tag_sRideAnimSet* set, void* b) {
    uint16_t heads;
    std::memcpy(&heads, set->mHeads->mF0C, sizeof heads);
    if (heads < 2) return;
    RideShape* shape = static_cast<RideShape*>(b);
    void** source = gHoardingSortSource;
    void** sorted = gHoardingSortScratch;
    int32_t* order = gHoardingSortOrder;

    const float halfWidth = static_cast<float>(shape->mWidth) * 0.5f;
    const float halfHeight = static_cast<float>(shape->mHeight) * 0.5f;
    const bool single = shape->mWidth == 1 && shape->mHeight == 1;

    float angles[20][20][4];
    uint32_t indices[20][20][4];
    int32_t count = 0;
    for (int32_t y = 0; y < shape->mHeight; ++y) {
        for (int32_t x = 0; x < shape->mWidth; ++x) {
            for (int32_t side = 0; side < 4; ++side) {
                if ((shape->mCells[y][x].mFlag & kSideBits[side]) != 0) {
                    angles[y][x][side] = HoardingCentreAngle(x, y, side, halfWidth, halfHeight);
                    source[count++] = &angles[y][x][side];
                } else {
                    angles[y][x][side] = kNoAngle;
                }
            }
        }
    }
    FloatSortByArray(count, source, sorted, 0, 0);

    int32_t next = 0;
    for (int32_t y = 0; y < shape->mHeight; ++y) {
        for (int32_t x = 0; x < shape->mWidth; ++x) {
            for (int32_t side = 0; side < 4; ++side) {
                if ((shape->mCells[y][x].mFlag & kSideBits[side]) == 0) continue;
                for (int32_t k = 0; k < count; ++k) {
                    if (sorted[k] == &angles[y][x][side]) {
                        indices[y][x][side] = static_cast<uint32_t>(k);
                        order[k] = next;
                    }
                }
                ++next;
            }
        }
    }

    auto* meshSet = static_cast<tag_sDMeshSet*>(MReleaseAllocF(sizeof(tag_sDMeshSet), 1));   // 0x40 on the Mac
    auto* entry = static_cast<sDMeshEntry*>(MReleaseAllocF(sizeof(sDMeshEntry), 1));         // 0x30 on the Mac
    auto* corners = static_cast<uint16_t*>(MReleaseAllocF(static_cast<uint32_t>(count) << 3, 1));
    const uint32_t quads = static_cast<uint32_t>(count) << 2;
    meshSet->mF00 = 3;
    meshSet->mCount = 1;
    meshSet->mEntries = entry;
    entry->mFlags = 4;
    entry->mFaceCount = static_cast<uint16_t>(quads);
    entry->mVertexCount = static_cast<uint16_t>(quads);
    entry->mPointCount = static_cast<uint16_t>(count);
    entry->mUVCount = static_cast<uint16_t>(quads);
    entry->mFaceSetCount = 1;
    entry->mBox[0] = -5.0f;
    entry->mBox[1] = -15.0f;
    entry->mBox[2] = -5.0f;
    entry->mBox[3] = ppc_fmadds(10.0f, static_cast<float>(shape->mWidth), 5.0f);
    entry->mBox[4] = 25.0f;
    entry->mBox[5] = ppc_fmadds(10.0f, static_cast<float>(shape->mHeight), 5.0f);

    char folder[1024];
    std::snprintf(folder, sizeof folder, gHoardingTexturePathFormat, theme);
    gRideHoardingTextures.mFolder = folder;   // left pointing at the buffer, as on the Mac
    DMCreateMesh(meshSet, &gRideHoardingTextures, "Hoardings", 1);
    SetMatrixIdentity(entry->mMatrix);
    sFaceSet* faceSet = entry->mFaceSets;
    faceSet->mTexture = gRideHoardingTextures.mTextures;
    faceSet->mFirstFace = 0;
    faceSet->mFaceCount = entry->mFaceCount;
    faceSet->mF08 = 0;
    faceSet->mF0A = count == 0 ? 0 : static_cast<uint16_t>(quads - 1);

    tag_sNode* node = set->mHeads->mRootNode;
    if (node == nullptr) return;

    // The node's vertices through its matrix.
    s3DPoint points[1024];
    const sMatrix& m = node->mMatrix;
    for (int32_t i = 0; i < static_cast<int32_t>(node->mVertexCount); ++i) {
        const sVertexPosnSet& v = node->mVertices[static_cast<uint32_t>(i) >> 2];
        const uint32_t lane = static_cast<uint32_t>(i) & 3;
        const float px = v.x[lane];
        const float py = v.y[lane];
        const float pz = v.z[lane];
        const float x = ppc_fmadds(pz, m.m[2][0], ppc_fmadds(px, m.m[0][0], py * m.m[1][0]));
        const float y = ppc_fmadds(pz, m.m[2][1], ppc_fmadds(px, m.m[0][1], py * m.m[1][1]));
        const float z = ppc_fmadds(pz, m.m[2][2], ppc_fmadds(px, m.m[0][2], py * m.m[1][2]));
        points[i].x = m.m[3][0] + x;
        points[i].y = m.m[3][1] + y;
        points[i].z = m.m[3][2] + z;
    }

    for (int32_t y = 0; y < shape->mHeight; ++y) {
        for (int32_t x = 0; x < shape->mWidth; ++x) {
            const float y0 = kWallHeight * static_cast<float>(y);
            const float x0 = kWallHeight * static_cast<float>(x);
            const float y1 = kWallHeight + y0;
            const float x1 = kWallHeight + x0;
            s2DPoint cell[4] = {{x0, y1}, {x1, y1}, {x1, y0}, {x0, y0}};
            s2DPoint insets[4];
            if (single) {
                insets[0] = {0.3f, -0.3f};
                insets[1] = {-0.3f, -0.3f};
                insets[2] = {0.3f, 0.3f};
                insets[3] = {0.3f, 0.3f};
            } else {
                DetermineInnerBaseInsets(cell, points, node->mVertexCount, insets);
            }
            auto in = [&insets](int k) { return (k & 1) != 0 ? insets[k >> 1].y : insets[k >> 1].x; };
            const uint8_t* flag = &shape->mCells[y][x].mFlag;
            uint8_t bits = *flag;
            if ((bits & 1) != 0) {
                const float a = (bits & 4) != 0 ? in(2) : 0.0f;
                const float c = (bits & 0x40) != 0 ? in(0) : 0.0f;
                const uint32_t index = indices[y][x][0];
                WriteSide(meshSet, index, quads, x1 + a, y1 + in(1), x0 + c, y1 + in(1), 0.0f, 1.0f);
                WriteCorners(corners, index, y + 1, x + 1, y + 1, x);
            }
            bits = *flag;
            if ((bits & 4) != 0) {
                const float a = (bits & 0x10) != 0 ? in(5) : 0.0f;
                const float c = (bits & 1) != 0 ? in(3) : 0.0f;
                const uint32_t index = indices[y][x][1];
                WriteSide(meshSet, index, quads, x1 + in(2), y0 + a, x1 + in(2), y1 + c, 1.0f, 0.0f);
                WriteCorners(corners, index, y, x + 1, y + 1, x + 1);
            }
            bits = *flag;
            if ((bits & 0x10) != 0) {
                const float a = (bits & 0x40) != 0 ? in(6) : 0.0f;
                const float c = (bits & 4) != 0 ? in(4) : 0.0f;
                const uint32_t index = indices[y][x][2];
                WriteSide(meshSet, index, quads, x0 + a, y0 + in(7), x1 + c, y0 + in(7), 0.0f, -1.0f);
                WriteCorners(corners, index, y, x, y, x + 1);
            }
            bits = *flag;
            if ((bits & 0x40) != 0) {
                const float a = (bits & 1) != 0 ? in(1) : 0.0f;
                const float c = (bits & 0x10) != 0 ? in(7) : 0.0f;
                const uint32_t index = indices[y][x][3];
                WriteSide(meshSet, index, quads, x0 + in(0), y1 + a, x0 + in(0), y0 + c, -1.0f, 0.0f);
                WriteCorners(corners, index, y + 1, x, y, x);
            }
        }
    }

    for (int32_t i = 0; i < count; ++i) {
        sUVSet& uv = meshSet->mUVSets[i];
        uv.mF00[0] = 0.0f;
        uv.mF00[4] = 1.0f;
        uv.mF00[1] = 1.0f;
        uv.mF00[5] = 1.0f;
        uv.mF00[2] = 0.0f;
        uv.mF00[6] = 0.0f;
        uv.mF00[3] = 1.0f;
        uv.mF00[7] = 0.0f;
    }

    for (int32_t i = 0; i < count; ++i) {
        sFace* face = meshSet->mFaces + static_cast<uint32_t>(i) * 4;
        const uint32_t base = (static_cast<uint32_t>(i) << 2) & 0xfffcu;
        const uint32_t back = (static_cast<uint32_t>(i) + quads) & 0xffffu;
        const uint16_t a = static_cast<uint16_t>(i * 4 + 1);
        const uint16_t b2 = static_cast<uint16_t>(i * 4 + 2);
        const uint16_t c = static_cast<uint16_t>(i * 4 + 3);
        face[0].mIndexAndFlag = static_cast<uint16_t>((face[0].mIndexAndFlag & 1) | ((base << 1) & 0xfffe));
        face[0].mF02[0] = static_cast<uint16_t>(base);
        face[0].mF02[1] = a;
        face[0].mF02[2] = b2;
        face[1].mIndexAndFlag = static_cast<uint16_t>((face[1].mIndexAndFlag & 1) | ((base << 1) & 0xfffe));
        face[1].mF02[0] = a;
        face[1].mF02[1] = c;
        face[1].mF02[2] = b2;
        face[2].mIndexAndFlag = static_cast<uint16_t>((face[2].mIndexAndFlag & 1) | ((back << 1) & 0xfffe));
        face[2].mF02[0] = b2;
        face[2].mF02[1] = a;
        face[2].mF02[2] = static_cast<uint16_t>(base);
        face[3].mIndexAndFlag = static_cast<uint16_t>((face[3].mIndexAndFlag & 1) | ((back << 1) & 0xfffe));
        face[3].mF02[0] = b2;
        face[3].mF02[1] = c;
        face[3].mF02[2] = a;
    }

    auto* geometry = static_cast<sHoardingGeometry*>(MReleaseAllocF(sizeof(sHoardingGeometry), 1));   // 0x10 on the Mac
    geometry->mF00[0] = static_cast<uint32_t>(count);
    geometry->mF00[1] = static_cast<uint32_t>(shape->mWidth);
    geometry->mF00[2] = static_cast<uint32_t>(shape->mHeight);
    geometry->mBlock = corners;
    set->mDynamicMeshSet = meshSet;
    set->mHoardingGeometry = geometry;
    set->mFlags |= 2;
}

// 0x10060ab8 (400 bytes). Angles other than 0, 90, 180, 270 leave both outputs alone.
void GetRotatedHFieldCoord(int32_t angle, float a1, float a2, float a3, float a4, float f5, float f6, uint32_t* outX, uint32_t* outZ) {
    const float one = 1.0f;
    const float bias = 0.1f;
    if (angle == 0xb4) {
        float t = a3 * (one - f5);
        *outX = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a1, f5, t)));
        t = a4 * (one - f6);
        *outZ = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a2, f6, t)));
    } else if (angle < 0xb4) {
        if (angle == 0x5a) {
            float t = a3 * f6;
            *outX = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a1, one - f6, t)));
            t = a4 * (one - f5);
            *outZ = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a2, f5, t)));
        } else if (angle == 0) {
            float t = a3 * f5;
            *outX = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a1, one - f5, t)));
            t = a4 * f6;
            *outZ = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a2, one - f6, t)));
        }
    } else if (angle == 0x10e) {
        float t = a3 * (one - f6);
        *outX = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a1, f6, t)));
        t = a4 * f5;
        *outZ = static_cast<uint32_t>(FloatToCell(bias + ppc_fmadds(a2, one - f5, t)));
    }
}

// 0x10060c80 (932 bytes). The set's hoardings (flag 2) get their vertex heights from the height field: for each hoarding i and each of its
// two corners, the cell (rotated by the mesh's angle) gives a height; vertices 4i and 4i+2, or 4i+1 and 4i+3, receive it.
void SetHoardingYOffsets(tag_sRideAnimSet* set) {
    if ((set->mFlags & 2) == 0) return;
    sHoardingGeometry* geometry = set->mHoardingGeometry;
    int32_t count = static_cast<int32_t>(geometry->mF00[0]);
    const uint8_t* node = reinterpret_cast<const uint8_t*>(static_cast<tag_sNode*>(set->mHeads->mHeader->mF78));
    float baseHeight = *reinterpret_cast<const float*>(node + 0x44);
    if (gHeightfieldHeader == nullptr) return;
    sHField* field = gHeightfieldHeader->mHField;
    if (field == nullptr) return;

    float c2 = static_cast<float>(set->mFC0.mF02);
    float c0 = static_cast<float>(set->mFC0.mF00);
    float c6 = static_cast<float>(set->mFC4.mF02);
    float c4 = static_cast<float>(set->mFC4.mF00);
    for (int32_t i = 0; i < count; i++) {
        for (int32_t j = 0; j < 2; j++) {
            geometry = set->mHoardingGeometry;
            const uint16_t* block = static_cast<const uint16_t*>(geometry->mBlock);
            uint32_t x = block[(i * 2 + j) * 2];
            uint32_t z = block[(i * 2 + j) * 2 + 1];
            int32_t angle = set->mHeads->mF10;
            float fz = static_cast<float>(z) / static_cast<float>(geometry->mF00[1]);
            float fx = static_cast<float>(x) / static_cast<float>(geometry->mF00[2]);
            uint32_t first = z;
            uint32_t second = x;
            GetRotatedHFieldCoord(angle, c2, c0, c6, c4, fz, fx, &first, &second);
            // (the original runs the four cases inline; the same operations, in the same order)
            uint32_t width = field->mF18[0];
            const float* heights = field->mHeights;
            float h = (heights[first + second * (width + 1)] - baseHeight) * 0.5f;
            sVertexPosnSet* sets = set->mDynamicMeshSet->mVertexSets;
            uint32_t base = static_cast<uint32_t>(i) & 0x3fffffff;
            if (j == 0) {
                sets[base].y[0] = 10.0f + h;
                sets[base].y[2] = h;
            } else {
                sets[base].y[1] = 10.0f + h;
                sets[base].y[3] = h;
            }
        }
    }
}

// The hoardings' face set takes the first texture word of the dynamic mesh's header, plus an offset per kind.
static void SetHoardingTexture(tag_sRideAnimSet* set, uint32_t add) {
    tag_sDMeshSet* dm = set->mDynamicMeshSet;
    dm->mEntries->mFaceSets->mTexture.mOffset = dm->mMesh->mHeader->mTextures.mOffset + add;
}

// 0x10061060 (456 bytes).
void EnableRideHoardings(tag_sRideAnimSet* set, int32_t enable) {
    if ((set->mFlags & 2) == 0) return;
    tag_sDMeshSet* dm = set->mDynamicMeshSet;
    if (enable != 0) {
        tag_sNode* node = set->mHeads->mHeader->mF78.get();
        sMatrix* matrix = dm->mEntries->mMatrix;
        SetMatrixIdentity(matrix);
        matrix->m[3][0] = node->mMatrix.m[3][0];
        matrix->m[3][1] = node->mMatrix.m[3][1];
        matrix->m[3][2] = node->mMatrix.m[3][2];
        float angle = 0.017453292f * static_cast<float>(set->mHeads->mF10);
        float a = (1.5707964f + angle) * gTPGlobals.mSinTableScale;
        float b = angle * gTPGlobals.mSinTableScale;
        int32_t ia = static_cast<int32_t>(a);
        int32_t ib = static_cast<int32_t>(b);
        float c = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ia)];
        float s = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ib)];
        YRotateLMatrixNoTrans(matrix, c, s);
        tag_sM3D2Header* from = set->mHeads->mHeader;
        tag_sM3D2Header* to = dm->mMesh->mHeader;
        to->mBox = from->mBox;
        DMEnableMeshRender(dm, set->mFC0, set->mFC4);
    } else {
        SetHoardingTexture(set, 0);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x100;
        DMDisableMeshRender(dm, set->mFC0, set->mFC4);
    }
}

// 0x10061268 (416 bytes).
void SetRideHoardingLevel(tag_sRideAnimSet* set, float level) {
    if ((set->mFlags & 2) == 0) return;
    uint32_t count = *static_cast<uint32_t*>(set->mHoardingMesh);
    sVertexPosnSet* vertices = set->mDynamicMeshSet->mVertexSets;
    sUVSet* uvs = set->mDynamicMeshSet->mMesh->mHeader->mF78.get()->mUVs.get();
    for (int32_t i = 0; i < static_cast<int32_t>(count); i++) {
        float scale = (i & 1) == 0 ? 1.0f : 0.8f;
        float q = static_cast<float>(i) / static_cast<float>(static_cast<int32_t>(count));
        float t = ppc_fnmsubs(4.0f, ppc_fmsubs(0.8f, q, level - 0.25f), 1.0f);
        if (t < 0.0f) t = 0.0f;
        if (1.0f < t) t = 1.0f;
        uint32_t base = static_cast<uint32_t>(i) * 4;
        uint32_t i1 = base + 1, i2 = base + 2, i3 = base + 3;
        float lift = 10.0f * t * scale;
        vertices[base >> 2].y[base & 3] = vertices[i2 >> 2].y[i2 & 3] + lift;
        float v = ppc_fnmsubs(scale, t, 1.0f);
        vertices[i1 >> 2].y[i1 & 3] = vertices[i3 >> 2].y[i3 & 3] + lift;
        uvs[i2 >> 2].mF00[i2 & 3] = 0.0f;
        uvs[i2 >> 2].mF00[4 + (i2 & 3)] = v;
        uvs[i3 >> 2].mF00[i3 & 3] = 1.0f;
        uvs[i3 >> 2].mF00[4 + (i3 & 3)] = v;
    }
}

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

// 0x10061448 (40 bytes).
void ClosedownRideHoardings() { DMFreeTextures(&gRideHoardingTextures); }

// 0x100614a0 (752 bytes).
void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) {
    if (set->mHoardingMesh == nullptr) return;
    if (flag == 1 && ((set->mFlags & 0x80) != 0 || (set->mFlags & 0xf00) == 0)) {
        SetHoardingTexture(set, 0);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x100;
    } else if (flag == 2 && (set->mFlags & 0x200) == 0) {
        SetHoardingTexture(set, 8);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x200;
    } else if (flag == 4 && (set->mFlags & 0x400) == 0) {
        SetHoardingTexture(set, 0x10);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x400;
    } else if (flag == 8 && (set->mFlags & 0x800) == 0) {
        SetHoardingTexture(set, 0x18);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x800;
    }
    set->mHoardingSpeed = 0.2f;
    if ((set->mFlags & 0x20) == 0) EnableRideHoardings(set, 1);
    set->mFlags |= 0x60;
    set->mFlags &= ~0x80u;
}

// 0x100617cc
void DeActivateHoardings(tag_sRideAnimSet* set) {
    if (set->mHoardingGeometry == nullptr) return;
    set->mHoardingSpeed = -0.30000001192092896f;
    if ((set->mFlags & 0x20) == 0) return;
    set->mFlags |= 0x80;
    set->mFlags &= ~0x40u;
}

// 0x10061840 (344 bytes).
void ReactivateHoardings(tag_sRideAnimSet* set) {
    if (set->mHoardingMesh == nullptr) return;
    if ((set->mFlags & 0x20) != 0) {
        EnableRideHoardings(set, 1);
        if ((set->mFlags & 0x40) != 0) {
            set->mHoardingSpeed = 0.2f;
        } else if ((set->mFlags & 0x80) != 0) {
            set->mHoardingSpeed = -0.3f;
        }
    }
    SetRideHoardingLevel(set, set->mHoardingLevel);
    uint32_t flags = set->mFlags;
    if ((flags & 0x100) != 0) {
        SetHoardingTexture(set, 0);
    } else if ((flags & 0x200) != 0) {
        SetHoardingTexture(set, 8);
    } else if ((flags & 0x400) != 0) {
        SetHoardingTexture(set, 0x10);
    } else if ((flags & 0x800) != 0) {
        SetHoardingTexture(set, 0x18);
    } else {
        SetHoardingTexture(set, 0);
        set->mFlags |= 0x100;
    }
}

// The ride mesh's frame callback and animation queries (SimTheme Park 2.04b3).
// TPRide.cpp: AnimateRideCallback 0x10069a98, ProcessRideAnim 0x1005b24c,
// HandleActiveHoardings 0x100619d4. rsse.cpp: GetCurrentAnimStats 0x100c7930.

// 0x100619d4, 216 bytes. Reaching an endpoint changes the flags on the next unchanged step.
void HandleActiveHoardings(tag_sRideAnimSet* set, float step) {
    if (set->mDynamicMeshSet == nullptr || set->mHoardingMesh == nullptr || (set->mFlags & 0xc0) == 0) return;
    const float old = set->mHoardingLevel;
    float level = ppc_fmadds(set->mHoardingSpeed, step, old);
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    set->mHoardingLevel = level;
    if (level == old) {
        if ((set->mFlags & 0x80) == 0) {
            if (level >= 1.0f) set->mFlags &= ~0x40u;
        } else if (level <= 0.0f) {
            set->mFlags &= ~0xa0u;
            EnableRideHoardings(set, 0);
        }
    } else {
        SetRideHoardingLevel(set, level);
    }
}
