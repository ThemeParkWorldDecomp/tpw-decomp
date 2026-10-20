// StripNodeWrapping (main app, SimTheme Park 2.04b3, ui_ErrorDisplay.cpp's range in the inventory; unit w81). StripRideWrapping
// (src/RideWrapping.cpp) calls it for each node of a ride mesh.
#include <cfloat>
#include <cmath>

#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"

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
