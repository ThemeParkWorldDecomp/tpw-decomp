// CalculateMaxVertexHeights 0x1005d304 (1984 bytes, TPRide.cpp): moves a node's vertices through the node's matrix, keeps them
// inside the ride's height map, widens the map's bounds, rasterises every face into the map keeping the highest value per
// cell, then does the same for the node's children.
#include <cstdint>

#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

int32_t gRideHeightStride;                 // 0x102c2144
float gRideHeightLimitZ;                   // 0x102c2148
float gRideHeightLimitX;                   // 0x102c214c
int32_t gRideHeightScan[50][4];            // 0x102c1e20

namespace {

// The error report shared by the clamps: remember the node, raise the flag.
inline void Report(tag_sNode* node, tag_sNode** badNode, int32_t* flag) {
    if (badNode != nullptr) *badNode = node;
    if (*flag == 0) *flag = 1;
}

// Clamps a coordinate into [0, limit): a value below -1 or above limit + 1 is an error (0 is the "ok" answer).
inline void ClampToLimit(float& value, const float& limit, tag_sNode* node, tag_sNode** badNode, int32_t* flag, int32_t& ok) {
    if (value < 0.0f) {
        if (value < -1.0f) {
            Report(node, badNode, flag);
            ok = 0;
        }
        value = 0.0f;
    } else if (value >= limit) {
        if (value > 1.0f + limit) {
            Report(node, badNode, flag);
            ok = 0;
        }
        value = limit - 0.001f;
    }
}

}  // namespace

int32_t CalculateMaxVertexHeights(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sRideHeightMap* map,
                                  tag_sNode** badNode, tag_sAnimHeader* anim, int32_t unused, int32_t* flag) {
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    int32_t ok = 1;
    if ((node->mFlags & 0x200) == 0 && (node->mFlags & 0x8010) == 0) {
        uint8_t* vertices = reinterpret_cast<uint8_t*>(node->mVertices.get());
        for (uint32_t i = 0, count = node->mVertexCount; count != 0; count--, i++) {
            float* block = reinterpret_cast<float*>(vertices + (i >> 2) * 0x30 + (i & 3) * 4);   // x, then y at +0x10, z at +0x20
            s3DPoint point;
            point.x = block[0];
            point.y = block[4];
            point.z = block[8];
            VectorThroughLMatrixNoW(&matrix, &point);
            if (point.x < map->mMin[0]) map->mMin[0] = point.x;
            if (point.y < map->mMin[1]) map->mMin[1] = point.y;
            if (point.z < map->mMin[2]) map->mMin[2] = point.z;
            if (point.x > map->mMax[0]) map->mMax[0] = point.x;
            if (point.y > map->mMax[1]) map->mMax[1] = point.y;
            if (point.z > map->mMax[2]) map->mMax[2] = point.z;
            ClampToLimit(point.x, gRideHeightLimitX, node, badNode, flag, ok);
            ClampToLimit(point.z, gRideHeightLimitZ, node, badNode, flag, ok);
            if (point.y < 0.0f) point.y = 0.0f;
            if (point.y >= 100.0f) {
                if (*flag == 0) *flag = 1;
                point.y = 99.999f;
            }
            block[0] = point.x;
            block[4] = point.y;
            block[8] = point.z;
        }

        const sFaceSet* faceSet = node->mFaceSets.get();
        for (uint32_t s = node->mFaceSetCount; s != 0; s--, faceSet++) {
            const sFace* face = node->mFaces.get() + faceSet->mFirstFace;
            for (uint32_t f = faceSet->mFaceCount; f != 0; f--, face++) {
                int32_t col[3], row[3], height[3];   // 0x9c, 0x84, 0x90
                for (int32_t k = 2; k >= 0; k--) {
                    const uint16_t index = node->mF94.get()[face->mF02[k]];
                    const float* b = reinterpret_cast<const float*>(vertices + (index >> 2) * 0x30 + (index & 3) * 4);
                    const float y = b[4];
                    float a = b[0] / 10.0f;
                    a = 5.0f * a;
                    col[k] = ppc_fctiwz(static_cast<double>(a));
                    float c = b[8] / 10.0f;
                    c = 5.0f * c;
                    row[k] = ppc_fctiwz(static_cast<double>(c));
                    float h = 255.0f * y;
                    h = h / 100.0f;
                    height[k] = ppc_fctiwz(static_cast<double>(h));
                }
                int32_t lo = row[0];
                if (row[1] < lo) lo = row[1];
                int32_t hi = row[0];
                if (row[1] > row[0]) hi = row[1];
                if (row[2] < lo) lo = row[2];
                if (row[2] > hi) hi = row[2];
                const int32_t lines = hi - lo + 1;
                for (int32_t n = 0; n < lines; n++) {
                    gRideHeightScan[n][0] = 1000000;
                    gRideHeightScan[n][1] = -1000000;
                }
                for (int32_t k = 2; k >= 0; k--) {
                    const int32_t nx = (k + 1) % 3;
                    const int32_t c0 = col[k], h0 = height[k];
                    const int32_t c1 = col[nx], h1 = height[nx];
                    const int32_t dRow = row[nx] - row[k];
                    const int32_t dCol = c1 - c0;
                    const int32_t dHeight = h1 - h0;
                    const int32_t half = dRow / 2;
                    int32_t step = 1;
                    int32_t span = dRow;
                    if (dRow < 0) {
                        step = -1;
                        span = -dRow;
                    }
                    int32_t* e = gRideHeightScan[row[k] - lo];
                    if (c0 < e[0]) {
                        e[0] = c0;
                        e[2] = h0;
                    }
                    if (c0 > e[1]) {
                        e[1] = c0;
                        e[3] = h0;
                    }
                    int32_t* e2 = e + dRow * 4;
                    if (c1 < e2[0]) {
                        e2[0] = c1;
                        e2[2] = h1;
                    }
                    if (c1 > e2[1]) {
                        e2[1] = c1;
                        e2[3] = h1;
                    }
                    for (int32_t i = 1; i < span; i++) {
                        e += step * 4;
                        const int32_t cv = c0 + (half + dCol * i) / span;
                        const int32_t hv = h0 + (dHeight * i) / span;
                        if (cv < e[0]) {
                            e[0] = cv;
                            e[2] = hv;
                        }
                        if (cv > e[1]) {
                            e[1] = cv;
                            e[3] = hv;
                        }
                    }
                }
                const int32_t* e = gRideHeightScan[0];
                for (int32_t n = 0; n < lines; n++, e += 4) {
                    const int32_t r = n + lo;
                    const int32_t c0 = e[0], c1 = e[1], h0 = e[2], h1 = e[3];
                    const int32_t width = c1 - c0;
                    const int32_t dHeight = h1 - h0;
                    uint8_t* cell = map->mCells.get() + (c0 + gRideHeightStride * r);
                    if (h0 > *cell) *cell = static_cast<uint8_t>(h0);
                    cell = map->mCells.get() + (c1 + gRideHeightStride * r);
                    if (h1 > *cell) *cell = static_cast<uint8_t>(h1);
                    for (int32_t j = 1; j < width; j++) {
                        const int32_t hv = h0 + (dHeight * j) / width;
                        cell = map->mCells.get() + (c0 + gRideHeightStride * r + j);
                        if (hv > *cell) *cell = static_cast<uint8_t>(hv);
                    }
                }
            }
        }
    }
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get()) {
        if (CalculateMaxVertexHeights(child, header, matrix, map, badNode, anim, unused, flag) == 0) ok = 0;
    }
    return ok;
}
