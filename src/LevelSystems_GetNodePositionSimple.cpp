// Unit v5258: GetNodePositionSimple, SetRideSpritePosnFocus, GetMeshPtrAtHFieldCell (TPRide.cpp).
#include <cstdint>

#include "LevelSystems.h"
#include "Ptr32.h"
#include "TPEngine.h"
#include "m3d2.h"

void SetRideSpritePosnFocus(tag_sMesh* mesh);   // include/rsse.h
extern Ptr32<uint8_t> gRideCtxRef;              // 0x102c1e10 (src/CalculateMaxVertexHeights_GetRideHeightAtPoint.cpp)
extern uint32_t gRideFlags;                     // 0x102c1e14

// 0x1005c8b8 (140 bytes).
void GetNodePositionSimple(tag_sNode* node, s3DPoint* out, int32_t) {
    if (node->mF04) {
        out->x = node->mMatrix.m[3][0];
        out->y = node->mMatrix.m[3][1];
        out->z = node->mMatrix.m[3][2];
    } else {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
    }
    for (tag_sNode* p = node->mF04; p != nullptr; p = p->mF04) {
        if (p->mF04) {
            out->x = out->x + p->mMatrix.m[3][0];
            out->y = out->y + p->mMatrix.m[3][1];
            out->z = out->z + p->mMatrix.m[3][2];
        }
    }
}

// 0x1005cc10 (156 bytes).
void SetRideSpritePosnFocus(tag_sMesh* mesh) {
    gRideCtxRef = reinterpret_cast<uint8_t*>(mesh);
    if (mesh->mF28 != nullptr && (*static_cast<uint32_t*>(mesh->mF28) & 8) != 0) {
        gRideFlags = 2;
        SetupMeshForPolySearchSimple(mesh, 0);
    } else if ((mesh->mHeader->mFlags & 0x80) != 0) {
        gRideFlags = 1;
    } else if (0.0f != mesh->mHeader->mFA0) {
        gRideFlags = 1;
    } else {
        gRideFlags = 4;
        SetupMeshForPolySearchSimple(mesh, 1);
    }
}

// 0x1005c1b0 (112 bytes).
tag_sRideAnimSet* GetMeshPtrAtHFieldCell(uint32_t x, uint32_t y) {
    sHField* field = gHeightfieldHeader->mHField;
    if (x >= field->mF18[0]) x = field->mF18[0] - 1;
    if (y >= field->mF18[1]) y = field->mF18[1] - 1;
    int32_t id = GetRideIDAtCell(static_cast<int32_t>(x), static_cast<int32_t>(y));
    if (id == 0) return nullptr;
    return gRideAnimSets[id];
}
