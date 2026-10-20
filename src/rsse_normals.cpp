// rsse.cpp (main app of SimTheme Park 2.04b3): the face normals of a mesh after an animation has moved its vertices
// (RecalculateInvalidNodeFaceNormals 0x100c7224, RecalculateInvalidFaceNormals 0x100c7418).
#include <cmath>
#include <cstring>

#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// The position of vertex `index` of a node: the vertices are kept four to a set (x, y and z of four vertices), the set
// being the index >> 2 and the lane the two low bits.
void VertexPosition(const sVertexPosnSet* sets, uint32_t index, float* position) {
    const sVertexPosnSet& set = sets[index >> 2];
    const uint32_t lane = index & 3;
    position[0] = set.x[lane];
    position[1] = set.y[lane];
    position[2] = set.z[lane];
}

}  // namespace

void RecalculateInvalidNodeFaceNormals(tag_sNode* node) {
    const uint16_t* remap = node->mF94.get();
    const sFace* face = node->mFaces.get();
    for (uint32_t count = node->mF5C; count != 0; --count, ++face) {
        // The face's second slot is the corner both edges start from.
        float origin[3];
        float first[3];
        float last[3];
        const sVertexPosnSet* sets = node->mVertices.get();
        VertexPosition(sets, remap[face->mF02[1]], origin);
        VertexPosition(sets, remap[face->mF02[0]], first);
        VertexPosition(sets, remap[face->mF02[2]], last);
        const float ux = first[0] - origin[0];
        const float uy = first[1] - origin[1];
        const float uz = first[2] - origin[2];
        const float vx = last[0] - origin[0];
        const float vy = last[1] - origin[1];
        const float vz = last[2] - origin[2];
        // The cross product v x u: each component is one exact product less a second one in a fused subtract, rounded to
        // double and then to single.
        s3DPoint* normal = node->mPoints.get() + (face->mIndexAndFlag >> 1);
        normal->x = static_cast<float>(ppc_fmsub(vy, uz, static_cast<double>(vz) * static_cast<double>(uy)));
        normal->y = static_cast<float>(ppc_fmsub(vz, ux, static_cast<double>(vx) * static_cast<double>(uz)));
        normal->z = static_cast<float>(ppc_fmsub(vx, uy, static_cast<double>(vy) * static_cast<double>(ux)));
        // The length: x * x is the exact product in double, the other two squares are rounded to single first; the sum
        // takes z * z first.
        const double xx = static_cast<double>(normal->x) * static_cast<double>(normal->x);
        const double yy = static_cast<double>(normal->y * normal->y);
        const double zz = static_cast<double>(normal->z * normal->z);
        const float length = static_cast<float>(std::sqrt(zz + (xx + yy)));
        normal->x = normal->x / length;
        normal->y = normal->y / length;
        normal->z = normal->z / length;
        // A face with no area has length 0 and a NaN for x (the class code 1 of the library's __fpclassifyf): it points down.
        if (std::isnan(normal->x)) {
            normal->x = 0.0f;
            normal->y = -1.0f;
            normal->z = 0.0f;
        }
    }
    node->mFlags &= ~0x10000u;
}

void RecalculateInvalidFaceNormals(tag_sM3D2Header* header) {
    tag_sNode* node = header->mNodes.get();
    for (uint32_t count = header->mNodeCount; count != 0; --count, ++node) {
        if ((node->mFlags & 0x10000) != 0) RecalculateInvalidNodeFaceNormals(node);
    }
    header->mFlags &= ~0x40000u;
}
