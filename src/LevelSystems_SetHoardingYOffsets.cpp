#include <cstdio>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

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
