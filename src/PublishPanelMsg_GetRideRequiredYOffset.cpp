// Ride placement on the height field (0x101ed058 .. 0x101edabc).
#include <cstdint>

#include "CellBase.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

// 0x101ed058, 232 bytes: the master bound box of a ride's cells, in world units.
int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    tag_sRideAnimSet* set = gRideAnimSets[ride];
    if (set == nullptr) return 0;
    const uint8_t* map = static_cast<const uint8_t*>(set->mHeightMap);
    if (map == nullptr) return 0;
    const float* lo = reinterpret_cast<const float*>(map + 0x1c);
    const float* hi = reinterpret_cast<const float*>(map + 0x28);
    float y = set->mHeads->mHeader->mF78.get()->mMatrix.m[3][1];
    const float ten = 10.0f;
    box->mMin[0] = ten * static_cast<float>(set->mFC0.mF02);
    box->mMin[1] = y + *lo;
    box->mMin[2] = ten * static_cast<float>(set->mFC0.mF00);
    box->mMax[0] = ppc_fmadds(ten, static_cast<float>(set->mFC4.mF02), ten);
    box->mMax[1] = y + *hi;
    box->mMax[2] = ppc_fmadds(ten, static_cast<float>(set->mFC4.mF00), ten);
    return 1;
}

// 0x101ed17c, 208 bytes: the node's vertices, transformed by `matrix`, into the scratch geometry buffer.
void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node, sMatrix* matrix) {
    float* out = static_cast<float*>(gScratchGeomBuffer);
    const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
    const sMatrix& m = *matrix;
    for (uint32_t i = 0; i < node->mVertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(verts + (i >> 2) * 0x30 + (i & 3) * 4);
        float x = p[0], y = p[4], z = p[8];
        out[0] = x;
        out[1] = y;
        out[2] = z;
        float ax = ppc_fmadds(x, m.m[0][0], y * m.m[1][0]);
        ax = ppc_fmadds(z, m.m[2][0], ax);
        float ay = ppc_fmadds(x, m.m[0][1], y * m.m[1][1]);
        ay = ppc_fmadds(z, m.m[2][1], ay);
        float az = ppc_fmadds(x, m.m[0][2], y * m.m[1][2]);
        az = ppc_fmadds(z, m.m[2][2], az);
        out[0] = m.m[3][0] + ax;
        out[1] = m.m[3][1] + ay;
        out[2] = m.m[3][2] + az;
        out += 3;
    }
}

// 0x101ed29c, 744 bytes: the middle of the highest and lowest height-field corner under the ride's cells.
float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation) {
    float fx = static_cast<float>(x);
    float fz = static_cast<float>(z);
    float f3, f4;
    if (rotation == 0 || rotation == 0xb4) {
        f3 = static_cast<float>(x) + static_cast<float>(w);
        f4 = static_cast<float>(z) + static_cast<float>(h);
    } else {
        f3 = static_cast<float>(x) + static_cast<float>(h);
        f4 = static_cast<float>(z) + static_cast<float>(w);
    }
    float stepW = 1.0f / static_cast<float>(w);
    float stepH = 1.0f / static_cast<float>(h);
    float maxV = -10000.0f;
    float minV = 10000.0f;
    float accW = 0.0f;
    for (int32_t i = 0; i < w; ++i) {
        float accH = 0.0f;
        for (int32_t j = 0; j < h; ++j) {
            bool use = true;
            if (a != nullptr) {
                int32_t cell = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(a) + j * 0xa0 + i * 8 + 8);
                use = CellIsRide(static_cast<CELL_MODES>(cell)) != 0 || (flags & 0x10) != 0;
            }
            if (use) {
                for (uint32_t k = 0; k < 4; ++k) {
                    float dz = (k & 2) ? stepH : 0.0f;
                    float dx = (k & 1) ? stepW : 0.0f;
                    uint32_t outX, outZ;
                    GetRotatedHFieldCoord(rotation, fx, fz, f3, f4, accW + dx, accH + dz, &outX, &outZ);
                    sHField* field = gHeightfieldHeader->mHField.get();
                    int32_t index = static_cast<int32_t>(outX) + static_cast<int32_t>(outZ) * static_cast<int32_t>(field->mF18[0] + 1);
                    float v = field->mHeights.get()[index];
                    if (v > maxV) maxV = v;
                    if (v < minV) minV = v;
                }
            }
            accH += stepH;
        }
        accW += stepW;
    }
    return (maxV + minV) * 0.5f;
}

// 0x101ed5bc, 180 bytes: the ride's corner after a turn of 90, 180 or 270 degrees.
void CorrectRidePosnForRotation(int32_t* x, int32_t* z, int32_t rotation, int32_t* outX, int32_t* outZ, int32_t w,
                                int32_t h) {
    if (rotation == 0xb4) {
        *x = *x + 1;
        *z = *z + 1;
        *outX = *x - w;
        *outZ = *z - h;
    } else if (rotation == 0x5a) {
        *z = *z + 1;
        *outX = *x;
        *outZ = *z - w;
    } else if (rotation == 0x10e) {
        *x = *x + 1;
        *outX = *x - h;
        *outZ = *z;
    } else {
        *outX = *x;
        *outZ = *z;
    }
}

// 0x101ed6ac, 676 bytes: sets the ride's cell range (flag 2) and its position and turn (flag 0x20).
void SetRidePositionAndRotation(tag_sRideAnimSet* set, int32_t outX, int32_t outZ, int32_t x, int32_t z,
                                int32_t rotation, float y, uint32_t flags, int32_t w, int32_t h) {
    TPGlobals* g = &gTPGlobals;
    int16_t shortW = static_cast<int16_t>(w);
    int16_t shortH = static_cast<int16_t>(h);
    if (flags & 2) {
        tag_sM3D2Header* header = set->mHeads->mHeader;
        int32_t spanX = w;
        if (rotation != 0 && rotation != 0xb4) {
            spanX = h;
            h = w;
        }
        header->mFlags |= 0x400;
        header->mFA8[3] = static_cast<uint8_t>(outX);
        header->mFA8[2] = static_cast<uint8_t>(outX + spanX - 1);
        header->mFA8[1] = static_cast<uint8_t>(outZ);
        header->mFA8[0] = static_cast<uint8_t>(outZ + h - 1);
        int16_t spanShortX = shortW;
        int16_t spanShortZ = shortH;
        if (rotation != 0 && rotation != 0xb4) {
            spanShortX = shortH;
            spanShortZ = shortW;
        }
        set->mFC0.mF02 = static_cast<uint16_t>(outX);
        set->mFC0.mF00 = static_cast<uint16_t>(outZ);
        set->mFC4.mF02 = static_cast<uint16_t>(static_cast<int16_t>(outX + spanShortX - 1));
        set->mFC4.mF00 = static_cast<uint16_t>(static_cast<int16_t>(outZ + spanShortZ - 1));
    }
    float fx = static_cast<float>(x);
    float fz = static_cast<float>(z);
    sHField* field = gHeightfieldHeader->mHField.get();
    float scaleA = field->mF08[2];
    float baseA = field->mF08[0];
    float scaleB = field->mF08[3];
    float baseB = field->mF08[1];
    if (flags & 0x20) {
        tag_sMesh* mesh = set->mHeads;
        mesh->mF10 = mesh->mF10 + rotation;
        mesh->mF10 = mesh->mF10 % 360;
        if (mesh->mF10 < 0) mesh->mF10 = mesh->mF10 + 360;
        float angle = 0.01745329238474369f * static_cast<float>(rotation);
        tag_sNode* node = mesh->mHeader->mF78.get();
        float c = g->mSinTable[g->mSinTableMask & static_cast<uint32_t>(static_cast<int32_t>((1.5707963705062866f + angle) * g->mSinTableScale))];
        float s = g->mSinTable[g->mSinTableMask & static_cast<uint32_t>(static_cast<int32_t>(angle * g->mSinTableScale))];
        YRotateLMatrixNoTrans(&node->mMatrix, c, s);
        node = mesh->mHeader->mF78.get();
        node->mMatrix.m[3][0] = ppc_fmadds(fx, scaleA, baseA);
        node->mMatrix.m[3][1] = y;
        node->mMatrix.m[3][2] = ppc_fmadds(fz, scaleB, baseB);
    }
}

// 0x101ed9a0, 288 bytes: moves a ride to cell (x, z).
void MoveRide(tag_sRideAnimSet* set, int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags) {
    if (set->mFlags & 0x4000)
        DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        DisableMeshDraw(set->mHeads);
    int32_t px = x, pz = z, outX, outZ;
    CorrectRidePosnForRotation(&px, &pz, set->mHeads->mF10, &outX, &outZ, w, h);
    float y = GetRideRequiredYOffset(px, pz, a, w, h, flags, set->mHeads->mF10);
    SetRidePositionAndRotation(set, px, pz, outX, outZ, 0, y, flags, w, h);
    set->mFE8 = x;
    set->mFEC = z;
    if (set->mFlags & 0x4000)
        EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        EnableMeshDraw(set->mHeads);
}
