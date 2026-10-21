// RidePlacement.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "CellBase.h"
#include "Debug.h"
#include "FS_System.h"
#include "InterfaceFont.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "SignRenderer.h"
#include "tb_mesh.h"
#include "tb_resource_texture.h"
#include "TbDynamicStringTemplate.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "World.h"
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Ride placement on the height field (0x101ed058 .. 0x101edabc).

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

// StripNodeWrapping (main app, SimTheme Park 2.04b3, ui_ErrorDisplay.cpp's range in the inventory; unit w81). StripRideWrapping
// (src/RideWrapping.cpp) calls it for each node of a ride mesh.

namespace {

// The node's texture coordinates are kept four vertices to a block of 0x20 bytes (sUVSet): the four u values, then the four v values.
// Vertex i is block i / 4, lane i % 4 (the Mac's byte offsets are (i & ~3) * 8 + (i & 3) * 4, and +0x10 for v).
float& U(const tag_sNode* node, uint32_t vertex) { return node->mUVs.get()[vertex >> 2].mF00[vertex & 3]; }
float& V(const tag_sNode* node, uint32_t vertex) { return node->mUVs.get()[vertex >> 2].mF00[4 + (vertex & 3)]; }

// What pass 1 does to the fractional part of one coordinate (the order of the Mac's tests, which differs from the decompiler's nest):
// a fraction below -(1 - amount) is taken to -1; one strictly between -amount and amount to 0; one above 1 - amount to 1; any other
// (including a NaN) is left alone. The caller adds the whole part back.
float Snap(float fraction, float amount, float complement) {
    if (fraction < -complement) return -1.0f;
    if (fraction > -amount && fraction < amount) return 0.0f;
    if (fraction > complement) return 1.0f;
    return fraction;
}

// A coordinate outside the unit square (a NaN is inside: every compare is false).
bool OutsideUnitSquare(float u, float v) { return u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f; }

}  // namespace

// 0x101edafc (1528 bytes). Four passes over one node.
//  1. Every vertex: u and v are split with modf into whole part and fraction, the fraction snapped (see Snap) and the whole part added back.
//  2. Three zeroed arrays are allocated: one word per vertex (visited), one float per face for the smallest u and one for the smallest v
//     over the face's three vertices (starting from FLT_MAX; a NaN never wins).
//  3. Every face: the whole part of its smallest u and v (one less when the fraction is negative) is taken off the u and v of those of its
//     vertices that no earlier face has already moved (the visited word marks them), so each face's lowest corner lies in [0, 1).
//  4. Every face set (the +0x6c records) over its faces. With the byte map `data` and the vertex positions `point` both given: the face's
//     centroid (the points are found through the vertex-to-point table at +0x94); for a face whose centroid height is strictly between -0.1 and
//     0.1 and whose map cell (centroid x / 10, centroid z / 10 truncated, 16 cells to a row) is non-zero, each vertex outside the unit
//     square gives the message once (`counter` guards it) and the vertex does not count; any other vertex counts as clear. With either missing,
//     only the unit-square check runs and nothing counts. A face set with no clear vertex at all and whose flag word (the ushort its first
//     pointer points to) lacks bit 0x8000 loses bits 0x30 of it.
//  The arrays are freed last (smallest v, smallest u, visited).
void StripNodeWrapping(tag_sM3D2Header* header, tag_sNode* node, float amount, void* data, s3DPoint* point, int32_t& counter) {
    for (uint32_t i = 0; i < node->mF5E; ++i) {
        const float u = U(node, i);
        const float v = V(node, i);
        double whole;
        const float uFraction = static_cast<float>(std::modf(static_cast<double>(u), &whole));
        const float uWhole = static_cast<float>(whole);
        const float complement = 1.0f - amount;
        const float uSnapped = uWhole + Snap(uFraction, amount, complement);
        const float vFraction = static_cast<float>(std::modf(static_cast<double>(v), &whole));
        const float vWhole = static_cast<float>(whole);
        const float vSnapped = vWhole + Snap(vFraction, amount, complement);
        U(node, i) = uSnapped;
        V(node, i) = vSnapped;
    }

    uint32_t* const visited = static_cast<uint32_t*>(MReleaseAllocF(static_cast<uint32_t>(node->mF5E) << 2, 1));
    float* const smallestU = static_cast<float*>(MReleaseAllocF(static_cast<uint32_t>(node->mF5C) << 2, 1));
    float* const smallestV = static_cast<float*>(MReleaseAllocF(static_cast<uint32_t>(node->mF5C) << 2, 1));

    const sFace* face = node->mFaces.get();
    for (uint32_t i = 0; i < node->mF5C; ++i, ++face) {
        float lowestU = FLT_MAX;
        float lowestV = FLT_MAX;
        for (uint32_t corner = 0; corner < 3; ++corner) {
            const uint32_t vertex = face->mF02[corner];
            const float u = U(node, vertex);
            const float v = V(node, vertex);
            if (u < lowestU) lowestU = u;
            if (v < lowestV) lowestV = v;
        }
        smallestU[i] = lowestU;
        smallestV[i] = lowestV;
    }

    face = node->mFaces.get();
    for (uint32_t i = 0; i < node->mF5C; ++i, ++face) {
        double whole;
        const float uFraction = static_cast<float>(std::modf(static_cast<double>(smallestU[i]), &whole));
        float uWhole = static_cast<float>(whole);
        const float vFraction = static_cast<float>(std::modf(static_cast<double>(smallestV[i]), &whole));
        float vWhole = static_cast<float>(whole);
        if (uFraction < 0.0f) uWhole = uWhole - 1.0f;
        if (vFraction < 0.0f) vWhole = vWhole - 1.0f;
        for (uint32_t corner = 0; corner < 3; ++corner) {
            const uint32_t vertex = face->mF02[corner];
            if (visited[vertex] == 0) {
                const float u = U(node, vertex);
                const float v = V(node, vertex);
                U(node, vertex) = u - uWhole;
                V(node, vertex) = v - vWhole;
                visited[vertex] = 1;
            }
        }
    }

    const uint8_t* const map = static_cast<const uint8_t*>(data);
    sFaceSet* faceSet = node->mFaceSets.get();
    for (uint32_t s = 0; s < node->mFaceSetCount; ++s, ++faceSet) {
        int32_t clear = 0;
        face = node->mFaces.get() + faceSet->mFirstFace;
        for (uint32_t f = 0; f < faceSet->mFaceCount; ++f, ++face) {
            float centreX = 0.0f;
            float centreY = 0.0f;
            float centreZ = 0.0f;
            if (map != nullptr && point != nullptr) {
                const uint16_t* const pointOf = node->mF94.get();
                const s3DPoint& first = point[pointOf[face->mF02[0]]];
                const s3DPoint& second = point[pointOf[face->mF02[1]]];
                const s3DPoint& third = point[pointOf[face->mF02[2]]];
                centreX = ((first.x + second.x) + third.x) / 3.0f;
                centreY = ((first.y + second.y) + third.y) / 3.0f;
                centreZ = ((first.z + second.z) + third.z) / 3.0f;
            }
            for (uint32_t corner = 0; corner < 3; ++corner) {
                const uint32_t vertex = face->mF02[corner];
                const float u = U(node, vertex);
                const float v = V(node, vertex);
                if (map != nullptr && point != nullptr) {
                    if (centreY > -0.1f && centreY < 0.1f) {
                        // (the Mac divides in single precision and truncates with fctiwz; the cell is x * 16 + z, a 32-bit sum)
                        const int32_t cellX = ppc_fctiwz(centreX / 10.0f);
                        const int32_t cellZ = ppc_fctiwz(centreZ / 10.0f);
                        const int32_t cell = static_cast<int32_t>(static_cast<uint32_t>(cellX) * 16u + static_cast<uint32_t>(cellZ));
                        if (map[cell] != 0) {
                            if (counter == 0 && OutsideUnitSquare(u, v)) {
                                ERRORBox("Ride %s has an invalid base, blending will not work\n", header->mName);
                                counter = 1;
                            }
                            continue;
                        }
                    }
                    ++clear;
                } else if (counter == 0 && OutsideUnitSquare(u, v)) {
                    ERRORBox("Ride %s has an invalid base, blending will not work\n", header->mName);
                    counter = 1;
                }
            }
        }
        if (clear == 0 && faceSet->mTexture.get() != nullptr) {
            uint16_t& flags = faceSet->mTexture->mF00;
            if ((flags & 0x8000) == 0) flags = static_cast<uint16_t>(flags & 0xffcf);
        }
    }

    MReleaseFreeF(smallestV);
    MReleaseFreeF(smallestU);
    MReleaseFreeF(visited);
}

// StripRideWrapping (main app, 0x101ee150, 236 bytes; the inventory files it under ui_ErrorDisplay.cpp; unit w75).

// Strips the wrapping of one node (`node` given) or of every node of the set's mesh header: node indices run over the header's node
// count at +0x42; an index below the full-node count at +0x44 is in the full nodes (stride 0xa0), the others are mini nodes (stride
// 0x58, counted from the difference); a node with flag 0x200 is skipped when walking (a node given is always done). All calls share
// one counter word, which starts at 0 here. The compares are unsigned (the counts are 16-bit).
void StripRideWrapping(tag_sRideAnimSet* set, tag_sNode* node, float amount, void* data, s3DPoint* point) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    int32_t counter = 0;
    if (node != nullptr) {
        StripNodeWrapping(header, node, amount, data, point, counter);
        return;
    }
    for (uint32_t index = 0; index < header->mAllNodeCount; ++index) {
        tag_sNode* current;
        if (index < header->mNodeCount)
            current = header->mNodes.get() + index;
        else
            current = static_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
        if ((current->mFlags & 0x200) == 0) StripNodeWrapping(header, current, amount, data, point, counter);
    }
}

// Ride signs (main app): 0x101ee290, 0x101eea14 and 0x101eed5c.

int32_t GenerateRideSign(tag_sRideAnimSet* set, char* folder, char* name, char* fontPath,
                         const char16_t* sign1, const char16_t* sign2) {
    SignRenderer renderer;
    if (sign1 == nullptr) return 0;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    char16_t text1[256];
    char16_t text2[256];
    if (sign2 != nullptr) {
        wcscpy(text1, sign1);
        wcscpy(text2, sign2);
    } else {
        const uint32_t length = wcslen(sign1);
        bool split = false;
        for (uint32_t i = 0; i < length; ++i) {
            const int32_t step = static_cast<int32_t>((i + 1) >> 1);
            const int32_t at = static_cast<int32_t>(length >> 1) + ((i & 1) ? -step : step);
            if (sign1[at] != u' ') continue;
            wcsncpy(text1, sign1, at);
            text1[at] = 0;
            wcsncpy(text2, sign1 + at + 1, length - at);
            split = true;
            break;
        }
        if (!split) {
            wcscpy(text1, sign1);
            text2[0] = 0;
        }
    }
    uint32_t indices[2] = {0xffffffff, 0xffffffff};
    sTextureInfo* infos[2] = {};
    sTexture* textures[2] = {};
    RideSignTextureRecord* records[2] = {};
    RideSignTextureRecord* block = nullptr;
    int32_t found = 0;
    bool needs = false;
    for (uint32_t i = 0; i < header->mTextureCount; ++i) {
        sTextureInfo& info = header->mTextureInfos[i];
        sTexture& texture = header->mTextures[i];
        int32_t which;
        if (info.mFlags & 8) which = 0;
        else if (info.mFlags & 0x10) which = 1;
        else {
            if ((info.mFlags & 2) || info.mFrames != 1) continue;
            if (NS_MacDoze::_stricmp(info.mName, "sign1.tga") == 0) which = 0;
            else if (NS_MacDoze::_stricmp(info.mName, "sign2.tga") == 0) which = 1;
            else continue;
            if (indices[which] != 0xffffffff) continue;
            if (block == nullptr) block = static_cast<RideSignTextureRecord*>(MReleaseAllocF(0x2c, 1));
            records[which] = block + which;
            records[which]->mWidth[0] = 0;
            records[which]->mWidth[1] = 0x80;
            records[which]->mHeight[0] = 0;
            records[which]->mHeight[1] = 0x80;
            info.mName = reinterpret_cast<char*>(records[which]);
            info.mFlags |= which == 0 ? 0xa : 0x12;
            texture.mF00 |= 0x480;
            needs = true;
        }
        ++found;
        texture.mF00 |= 0x400;
        records[which] = reinterpret_cast<RideSignTextureRecord*>(info.mName.get());
        if (!(records[which]->mFlags & 2)) needs = true;
        indices[which] = i;
        infos[which] = &info;
        textures[which] = &texture;
    }
    auto fail = [&]() {
        for (int32_t i = 0; i < 2; ++i) {
            if (infos[i] != nullptr) infos[i]->mFlags &= ~2u;
            if (textures[i] != nullptr) textures[i]->mF00 &= ~0x80u;
        }
        if (block != nullptr) MReleaseFreeF(block);
        return 0;
    };
    if (found != 2) {
        if (found == 1) ERRORBox("Ride %s does not have correctly set up signs\n", header->mName);
        return fail();
    }
    int32_t result = 1;
    if (needs) {
        tag_sMesh* mesh = set->mHeads;
        CMesh* engine = mesh->mEngineMesh != nullptr ? mesh->mEngineMesh : mesh->mTypeEngineMesh;
        result = engine->GenerateRideSign(indices, 2);
        records[0]->mData.mOffset = indices[0];
        records[1]->mData.mOffset = indices[1];
        records[0]->mFlags |= 2;
        records[1]->mFlags |= 2;
    }
    if (result == 0) return fail();
    auto findNode = [&](sTexture* texture) -> tag_sNode* {
        tag_sM3D2Header* h = set->mHeads->mHeader;
        for (uint32_t i = 0; i < h->mAllNodeCount; ++i) {
            tag_sNode& node = h->mNodes[i];
            for (uint32_t j = 0; j < node.mFaceSetCount; ++j)
                if (node.mFaceSets[j].mTexture.get() == texture) return &node;
        }
        return nullptr;
    };
    tag_sNode* node1 = findNode(textures[0]);
    tag_sNode* node2 = findNode(textures[1]);
    StripRideWrapping(set, node1, 0.02f, nullptr, nullptr);
    if (node2 != node1) StripRideWrapping(set, node2, 0.02f, nullptr, nullptr);
    char folderCopy[1040];
    char settings[1024];
    std::strcpy(folderCopy, folder);
    const size_t length = std::strlen(folderCopy);
    if (length != 0 && folderCopy[length - 1] == ':') folderCopy[length - 1] = 0;
    if (name == nullptr) {
        char* colon = std::strrchr(folderCopy, ':');
        name = colon != nullptr ? colon + 1 : folderCopy;
    }
    std::sprintf(settings, "%s:%s_%s.sgn", folderCopy, gText.GetLanguageName(), name);
    if (CheckFileExists(settings) == 0) std::sprintf(settings, "%s:%s.sgn", folderCopy, name);
    renderer.sign_load_settings(settings);
    renderer.sign_install_fonts(fontPath);
    const int32_t format = gei_TextureSettings.mFormat;
    sign_renderer_options options = {text1, text2, records[0]->mData.get(), records[1]->mData.get(),
                                      0x80, 0x80, (format <= 2 || format == 4 || format >= 6) ? 2 : 1};
    auto* dc = NS_MacDoze::GetDC(gMainWindow);
    renderer.sign_render(dc, options);
    NS_MacDoze::ReleaseDC(gMainWindow, dc);
    header->mFlags |= 0x8000;
    if (!(set->mFlags & 1)) set->mFlags |= 0x8000000;
    return 1;
}

void DestroyRideSign(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    if (!(header->mFlags & 0x8000)) return;
    for (uint32_t i = 0; i < header->mTextureCount; ++i) {
        sTextureInfo& info = header->mTextureInfos[i];
        if (!(info.mFlags & 8)) continue;
        MReleaseFreeF(info.mName.get());
        info.mName = nullptr;
        return;
    }
}

// A ride's working copy of its animation set (the main app's TPRide.cpp, TPEmbed.cpp, rsse.cpp and
// ui_ErrorDisplay.cpp ranges, as the inventory files them): InstanceRideAnimSet and what it calls
// (InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its sibling), and the
// small functions LoadRideType calls on a loaded set (CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads,
// GetFontPath, OverrideActiveChannels) with ERRORBox.
//
// The copies are made the way the original makes them: one block from the allocator (MReleaseAllocF), the
// pieces laid after each other in it, then every pointer that led into the original's tables made to lead
// into the copies. The mesh and animation records keep the file's layout (Ptr32 fields, arena offsets), so
// the original's arithmetic on addresses is arithmetic on offsets here. The game's own records (tag_sMesh,
// the embedded set and its records, tag_sRideAnimSet and its entries, tag_sDMeshSet) have native pointers
// and are larger than on the Mac, so the block's fixed parts are larger too (see InstanceMesh).

// 0x101eeac8, 176 bytes. The folder of the game's fonts: the local path, a ':' unless it already ends in
// one, and "Data:Fonts". 0 (and `out` untouched) when there is no local path. For an empty path the original
// reads the byte before the string; here it counts as not a ':'.
int32_t GetFontPath(char* out) {
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    const size_t length = std::strlen(path);
    if (length != 0 && path[length - 1] == ':') std::sprintf(out, "%s%s", path, "Data:Fonts");
    else std::sprintf(out, "%s:%s", path, "Data:Fonts");
    return 1;
}

// Unit v5934. The object list's end marker (halfword at 0x102b12e6; its neighbour 0x102b12e8 is the GetAnimStats jump table).
uint16_t gRideSignListEnd = 0;

namespace {
inline uint32_t WordAt(const void* base, size_t offset) {
    return *reinterpret_cast<const uint32_t*>(static_cast<const char*>(base) + offset);
}
inline uint16_t HalfAt(const void* base, size_t offset) {
    return *reinterpret_cast<const uint16_t*>(static_cast<const char*>(base) + offset);
}
}  // namespace

// 0x101eeb9c (400 bytes). The object is reached by offset (+0x1c the ride instance). Answers 0 for no record, no instance or no local
// path, 1 when the renderer is software (nothing drawn), else GenerateRideSign's answer; the set is flagged 0x8000000 (when its bit 0
// is clear) in every case that got that far. When UniStringToString answers 0 the original answers its caller's r31; here 0.
void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB) {
    RegenerateRideSignResult(object, signA, signB);
}

int32_t RegenerateRideSign(uint32_t instance, char* folder, char* name,
                           const char16_t* sign1, const char16_t* sign2) {
    if (instance == 0) return 0;
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    char fontPath[1032];
    const size_t length = std::strlen(path);
    std::sprintf(fontPath, length != 0 && path[length - 1] == ':' ? "%s%s" : "%s:%s", path, "Data:Fonts");
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    const int32_t result = gPlayerSettings.mCurrent.mRenderer == 0 ? 1 :
        GenerateRideSign(set, folder, name, fontPath, sign1, sign2);
    if (set != nullptr && !(set->mFlags & 1)) set->mFlags |= 0x8000000;
    return result;
}

int32_t RegenerateRideSignResult(void* object, const char16_t* signA, const char16_t* signB) {
    const uint32_t instance = WordAt(object, 0x1c);
    RideInfo* info = static_cast<CObject*>(object)->GetDBInfo();
    if (info == nullptr || instance == 0) return 0;
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    char fontPath[1024];
    const size_t length = std::strlen(path);
    if (path[length - 1] == ':') std::sprintf(fontPath, "%s%s", path, "Data:Fonts");
    else std::sprintf(fontPath, "%s:%s", path, "Data:Fonts");
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    int32_t result = 0;
    if (gPlayerSettings.mCurrent.mRenderer == 0) {
        result = 1;
    } else {
        TbDynamicStringTemplate<char16_t> wide(info->mItemName);
        TbDynamicStringTemplate<char> narrow;
        if (UniStringToString(narrow, wide) != 0)
            result = GenerateRideSign(set, info->mFolder, const_cast<char*>(narrow.mData), fontPath, signA, signB);
    }
    if (set != nullptr && (set->mFlags & 1) == 0) set->mFlags |= 0x8000000;
    return result;
}

// 0x101eeeb4 (240 bytes). Walks the object list (head: the world's halfword +0x1da746, links at +0xc of each object, ends at the marker)
// and regenerates the sign of each ride whose set has flag 0x8000000.
void RegenerateRideSigns() {
    char16_t signA[256];
    char16_t signB[256];
    uint16_t id = gGameWorld->mF1DA746;
    while (id != gRideSignListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        CObject* object = static_cast<CObject*>(gThingArray[id].mThing);
        tag_sRideAnimSet* set = gRideAnimSets[WordAt(object, 0x1c)];
        if (set != nullptr && (set->mFlags & 0x8000000) != 0) {
            object->GetSignNameA(signA, 0x100);
            object->GetSignNameB(signB, 0x100);
            RegenerateRideSign(object, signA, signB);
        }
        id = HalfAt(object, 0xc);
    }
}

// 0x101eefd0 (76 bytes). No NULL check on the set: the instance's set is dereferenced.
void SetRideScriptID(uint32_t instance, uint32_t script) {
    if (instance == 0) return;
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    set->mFD4 = static_cast<int32_t>(script);
    if ((set->mFlags & 0x40000) != 0) SetCoasterScriptID(instance, static_cast<int32_t>(script));
}

// 0x101ef044 (300 bytes). Finds the group (of the first 12) and the place in it whose entry holds the set's animation number `index`;
// answers the group's name ("Failure" when none).
void GetAnimStats(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other) {
    GetAnimStatsName(set, index, kind, place, other);
}

// Leaves the park's second-stage set-up calls (main app; unit w112a): the coaster track extruder's storage, the object instance
// table's set-up, the texture set loader and freer of the dynamic meshes (TPAdvisor.cpp range of the inventory) and the two
// static texture sets with their entries (initialised data of the image).

// 0x101ef1b0 (12 bytes): only the third counter is written (the object table's "lowest" word); the first two are not touched.
void SetupObjectInstanceTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest) { lowest = 1; }

const char* GetAnimStatsName(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other) {
    static const char* const kNames[15] = {"Create", "Destroy", "Idle", "Load", "Start", "Main", "End", "Unload",
                                           "Worn", "Break", "Repair", "Other", "None", "Pause", "Failure"};
    for (int32_t group = 0; group < 12; ++group) {
        const sRideAnimGroup& g = set->mGroups[group];
        for (uint32_t i = 0; i < g.mCount; ++i) {
            if (set->mAnimations[index] == g.mEntries[i].mAnimation) {
                if (kind != nullptr) *kind = static_cast<uint32_t>(group);
                if (place != nullptr) *place = i;
                if (other != nullptr) *other = g.mCount;
                if (static_cast<uint32_t>(group) < 15) return kNames[group];
            }
        }
    }
    return "Failure";
}

// 0x101ef1f4 (212 bytes). count: live entries, firstFree: holes below the top, lowest: the next free slot. `extra` other than
// 0xffffffff just stores at that slot. Answers 0 when the table is full (no hole and count at the limit).
uint32_t AllocateObjectInstanceToTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, uint32_t flags, void** table,
                                       void* object, uint32_t extra) {
    if (extra != 0xffffffffu) {
        table[extra] = object;
        return extra;
    }
    if (firstFree == 0 && count >= flags) return 0;
    const uint32_t slot = lowest;
    table[slot] = object;
    const uint32_t top = count;
    if (lowest - 1 == top) {
        lowest = lowest + 1;
        count = count + 1;
    } else {
        for (uint32_t i = lowest; i <= top + firstFree; ++i) {
            if (table[i + 1] == nullptr) {
                lowest = i + 1;
                break;
            }
        }
        firstFree = firstFree - 1;
        count = count + 1;
    }
    return slot;
}

// 0x101ef310 (220 bytes).
void RemoveObjectInstanceFromTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, void** table, uint32_t id) {
    if (id == 0) return;
    table[id] = nullptr;
    const uint32_t holes = firstFree;
    if (holes == 0) {
        if (id == count) {
            lowest = lowest - 1;
            count = count - 1;
            return;
        }
        firstFree = holes + 1;
        lowest = id;
        count = count - 1;
        return;
    }
    const uint32_t top = count;
    if (id == top + holes) {
        count = top - 1;
        for (;;) {
            if (firstFree == 0) return;
            if (table[static_cast<uint32_t>(count + firstFree)] != nullptr) return;
            firstFree = firstFree - 1;
        }
    }
    firstFree = holes + 1;
    count = count - 1;
    if (id >= lowest) return;
    lowest = id;
}

// The small leaves the lobby's start reaches (main app, SimTheme Park 2.04b3): the theme texture folders, the authorised
// user text, the interface's built-in debug font and the interface string heap. The inventory files the first two under
// ui_ErrorDisplay.cpp, Gen8x8DebugFont under FS_MasterSave.cpp and Allocate under ui_cityLobbyController.cpp: the ranges
// do not match their names.

// ---- the theme texture folders

char gThemeTextureFolder[16];    // 0x103963cc
char gSharedTextureFolder[16];   // 0x103963bc

// 0x101ef430
void SetThemeTextureDirectories(int32_t small) {
    if (small != 0) {
        std::snprintf(gThemeTextureFolder, sizeof(gThemeTextureFolder), "stexture");
        std::snprintf(gSharedTextureFolder, sizeof(gSharedTextureFolder), "ssharete");
    } else {
        std::snprintf(gThemeTextureFolder, sizeof(gThemeTextureFolder), "textures");
        std::snprintf(gSharedTextureFolder, sizeof(gSharedTextureFolder), "sharetex");
    }
}

// ---- the authorised user

// The text the check compares and decodes (data of the main app: two strings and a table in it). Nothing writes them.
char gSecurityKey[] = "QPALZMWOSKXNThe Quick Brown Fox Jumped over the Lazy dog ... blah blah blah.";   // 0x1029d311
char gSecurityText[] = "he Quick Brown Fox Jumped over the Lazy dog ... blah blah blah.";                // 0x1024f351
char gSecurityMask[] = "PQPOEDIJKMCIOOSPLADTOSZAFKLASDJPOYCGFKHGYGHEJLFPZVLKFLIPQWDLFOLFMIE";             // 0x102b1324
char gAuthorizationStamp[] = "No Authorization Stamp";                                                    // 0x102b1368
char gAuthorizedUser[80];                                                                                 // 0x103963dc

// 0x101ef4e0: "No Authorization Stamp", unless the key's text (from its 13th character) differs from the one in the string
// table, which no shipped copy does. Then the answer is a text worked out from the key: the difference between the key's
// characters (from the 12th) and the mask's, up to the first place where they agree. (The loop keeps no terminator of its
// own and stores nothing for the place where the two agree: the buffer's zeros end the text.)
const char* SEC_AuthorizedUser() {
    uint32_t i = 0;
    if (std::strcmp(gSecurityKey + 0xd, gSecurityText) != 0) {
        for (;;) {
            const int32_t difference = static_cast<int32_t>(static_cast<char>(gSecurityKey[i + 0xc])) -
                                       static_cast<int32_t>(static_cast<char>(gSecurityMask[i]));
            if (difference == 0) break;
            gAuthorizedUser[i] = static_cast<char>(difference);
            ++i;
        }
        return gAuthorizedUser;
    }
    return gAuthorizationStamp;
}
