// GenerateRideHeightMap 0x1005db4c (1892 bytes, TPRide.cpp): the height map of a ride. It instances the ride's animation set,
// takes the vertices' highest point per cell of a five-by-five finer grid (CalculateMaxVertexHeights) either once, or, when the
// set has animations other than the two first kinds, once per 30 ms step of every animation, then reduces each cell to the
// highest of its 25 finer cells. Answers the map, or NULL when the set cannot be instanced.
#include "TPEngine.h"
#include "FS_System.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "CBalanceTable.h"
#include "rsse.h"
#include "ppc_math.h"

// 0x102c2140: the second word of the pair the heights code reads, the map's depth in finer cells (height times five).
int32_t gRideHeightRows;

namespace {

// The map's word at 0x08 holds two halves: the width at 0x08 and the depth at 0x0a, in the byte order of the machine.
uint16_t* MapHalves(sRideHeightMap* map) { return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(map) + 8); }

void SetLimits(sRideHeightMap* map) {
    const uint16_t* halves = MapHalves(map);
    gRideHeightLimitX = 10.0f * static_cast<float>(halves[0]);
    gRideHeightLimitZ = 10.0f * static_cast<float>(halves[1]);
    gRideHeightStride = static_cast<int32_t>(halves[0]) * 5;
    gRideHeightRows = static_cast<int32_t>(halves[1]) * 5;
}

}  // namespace

sRideHeightMap* GenerateRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, RideShape* shape, int32_t flag) {
    tag_sRideAnimSet* inst = nullptr;
    if (InstanceRideAnimSet(set, nullptr, 0x211f, &inst, 0) == 0) return nullptr;
    InitRideAnimSet(inst, 1, 0, 0);
    tag_sM3D2Header* instHeader = inst->mHeads->mHeader;   // read again after the instance is destroyed

    sRideHeightMap* map = static_cast<sRideHeightMap*>(MReleaseAllocF(0x30, 1));
    map->mF00 = 0xab1e0003u;
    map->mF04 = 0x640005u;
    MapHalves(map)[0] = static_cast<uint16_t>(w);
    MapHalves(map)[1] = static_cast<uint16_t>(h);
    const uint32_t cellCount = w * h;
    const uint32_t fineBytes = w * 5 * h * 5;   // five by five finer cells per cell, a byte each
    map->mCells = static_cast<uint8_t*>(MReleaseAllocF(fineBytes + cellCount * 2, 1));
    map->mF10 = map->mCells.mOffset + fineBytes;
    map->mF14 = map->mF10 + cellCount;

    int32_t flagWord = 0;
    tag_sNode* badNode = nullptr;
    bool animated = false;
    if (inst->mAnimationCount != 0) {
        for (uint32_t kind = 0; kind < 12; kind++) {
            if (kind != 0 && kind != 1 && inst->mGroups[kind].mCount != 0) animated = true;
        }
    }

    if (animated) {
        map->mMin[0] = 3.4028234663852886e+38f;
        map->mMin[1] = 3.4028234663852886e+38f;
        map->mMin[2] = 3.4028234663852886e+38f;
        map->mMax[0] = 1.1754943508222875e-38f;
        map->mMax[1] = 1.1754943508222875e-38f;
        map->mMax[2] = 1.1754943508222875e-38f;
        for (uint32_t i = 0; i < inst->mAnimationCount; i++) {
            tag_sAnimHeader* anim = inst->mAnimations[i];
            if (anim == nullptr) continue;
            if (inst->mGroups[0].mEntries != nullptr && anim == inst->mGroups[0].mEntries->mAnimation) continue;
            if (inst->mGroups[1].mEntries != nullptr && anim == inst->mGroups[1].mEntries->mAnimation) continue;

            uint32_t time = 0;
            uint32_t kind, index, unused;
            sAnimTimeControl control;
            GetAnimStats(inst, i, &kind, &index, &unused);
            control.mF04 = 0xc;
            control.mF08 = 0;
            control.mF0C = 1.0f;
            SetRideAnimTime(time);
            ResetAnimTimeStats(&control, anim, 1, 0.0f);
            GetAnimTimeStats(&control, &time);   // the original passes a third argument, 1, which the function ignores
            SetNewAnim(inst, kind, index, &control, 0, 1.0f);
            while (GetAnimRemainingMilliseconds(&control) >= 0.0f) {
                RATick(&control, inst->mHeads, anim);
                const int32_t steps = ppc_fctiwz(control.mF20);
                tag_sMesh* mesh = inst->mHeads;
                SetLimits(map);
                sMatrix matrix;
                SetMatrixIdentity(&matrix);
                CalculateMaxVertexHeights(mesh->mHeader->mF78, mesh->mHeader, matrix, map, &badNode, anim, steps, &flagWord);
                QuickCopyUnmaskedMeshVertices(inst->mHeads->mF08, inst->mHeads, 1);
                tag_sNode* node = instHeader->mNodes;
                for (uint32_t n = instHeader->mNodeCount; n != 0; n--) {
                    node->mFlags &= 0xff7fffffu;
                    node++;
                }
                time += 0x1e;
                SetRideAnimTime(time);
                GetAnimTimeStats(&control, &time);
            }
            ResetRideAnimationSingleChannel(inst, inst->mGroups[control.mF04].mEntries[control.mF08].mAnimation, nullptr);
        }
    } else {
        tag_sM3D2Header* original = set->mHeads->mHeader;
        std::memcpy(map->mMin, original->mBox.mMin, sizeof(map->mMin));
        std::memcpy(map->mMax, original->mBox.mMax, sizeof(map->mMax));
        tag_sMesh* mesh = inst->mHeads;
        SetLimits(map);
        sMatrix matrix;
        SetMatrixIdentity(&matrix);
        CalculateMaxVertexHeights(mesh->mHeader->mF78, mesh->mHeader, matrix, map, &badNode, nullptr, 0, &flagWord);
    }
    DestroyRideAnimSetInstance(inst, nullptr, 0);

    uint8_t* fine = map->mCells;
    Ptr32<uint8_t> heights;
    heights.mOffset = map->mF10;
    Ptr32<uint8_t> solid;
    solid.mOffset = map->mF14;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t best = 0;
            const uint32_t base = x * 5 + w * (y * 0x19);
            for (uint32_t row = 0; row < 5; row++) {
                for (uint32_t col = 0; col < 5; col++) {
                    const uint8_t value = fine[base + (col + w * (row * 5))];
                    if (value > best) best = value;
                }
            }
            if (flag != 0)
                heights[x + y * w] = static_cast<uint8_t>(ppc_fctiwz((255.0f * instHeader->mBox.mMax[1]) / 100.0f));
            else
                heights[x + y * w] = best;
            if (shape == nullptr || shape->mCells[y][x].mValue != 0)
                solid[x + y * w] = 1;
            else
                solid[x + y * w] = 0;
        }
    }
    return map;
}
