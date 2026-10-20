// DestroyTrackBrace 0x1004094c (308 bytes) and PlaceEntrancePylons 0x10040acc (764 bytes) of TPCoaster.cpp.
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"

// 0x1004094c: frees a brace's blocks and the brace; the pylon data's flag 1 keeps the shared 0x7c and 0x88 blocks.
void DestroyTrackBrace(tag_sTrackBrace* brace, sPylonControl* pylon) {
    if (brace->mWord70 != 0) {
        DestroyRideInstance(brace->mWord70);
        brace->mWord70 = 0;
    }
    if (pylon != nullptr && (pylon->mFlags & 8) != 0) {
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk7C);
        MReleaseFreeF(brace->mBlk80);
        brace->mBlk7C = nullptr;
        brace->mBlk80 = nullptr;
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk88);
        MReleaseFreeF(brace->mBlk8C);
        brace->mBlk88 = nullptr;
        brace->mBlk8C = nullptr;
    } else {
        if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(brace->mBlk7C);
        MReleaseFreeF(brace->mBlk80);
        brace->mBlk7C = nullptr;
        brace->mBlk80 = nullptr;
        brace->mBlk88 = nullptr;
        brace->mBlk8C = nullptr;
    }
    if (GetCurSelectedTrackBrace() == brace) SetCurSelectedTrackBrace(nullptr);
    MReleaseFreeF(brace);
}

// 0x10040acc: the entrance's two pylons (three with hoardings) and their braces, then the end pylon; nothing is made when the instance is the editor's (mF40).
void PlaceEntrancePylons(tag_sCoasterInstance* instance, uint32_t x, uint32_t z, float height, int32_t flag) {
    (void)x;
    (void)z;
    (void)height;
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    int32_t count, first;
    if ((type->mFlags & 0x20) != 0) {
        count = 3;
        first = 1;
    } else {
        count = 2;
        first = 2;
    }
    if (instance->mF40 == 0) {
        sPylonControl* control = &type->mPylonsA;
        uint32_t flags = 0;
        float local[3];
        for (int32_t i = 0; i < count; ++i) {
            tag_sRideAnimSet* set = gRideAnimSets[instance->mEntranceID];
            const uint32_t index = FindDataNodeIndexByID(set, 0x400000, static_cast<uint32_t>(i + first));
            if (index == 0xffffffffu) continue;
            set = gRideAnimSets[instance->mEntranceID];
            const sEmbeddedSet* table = static_cast<const sEmbeddedSet*>(set->mHeads->mF28);
            const uint8_t* block = table->mRecords[index].mBlock;
            if (block != nullptr) {
                const float* p = reinterpret_cast<const float*>(block + 0x30);
                local[0] = p[0];
                local[1] = p[1];
                local[2] = p[2];
            } else {
                local[0] = local[1] = local[2] = 0.0f;
            }
            if ((type->mFlags & 0x20) != 0 && (i == 0 || i == 2)) {
                set = gRideAnimSets[instance->mEntranceID];
                const uint8_t* node = reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get());
                local[1] = local[1] - *reinterpret_cast<const float*>(node + 0x44);
                const float ten = 10.0f;
                const uint32_t cellX = static_cast<uint32_t>(FloatToCell(static_cast<double>(local[0] / ten)));
                const uint32_t cellZ = static_cast<uint32_t>(FloatToCell(static_cast<double>(local[2] / ten)));
                sPylonRecord* pylon = NewPylon(cellX, cellZ, 0x12, 0, control, nullptr);
                if (i == 0)
                    flags = 0x274;
                else if (i == 2)
                    flags = 0x28c;
                if (flag != 0) flags |= 1;
                tag_sTrackBrace* brace = NewTrackBrace(pylon, 0.0f, 0.0f, local[1], flags, control, 0, 0, 0);
                SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), brace);
            } else {
                sPylonRecord* pylon = NewPylon(0, 0, 0x13, 0, nullptr, reinterpret_cast<s3DPoint*>(local));
                flags = flag != 0 ? 1 : 0;
                if ((type->mFlags & 0x20) != 0) flags |= 0x3c;
                NewTrackBrace(pylon, pylon->mX, pylon->mZ, 0.0f + pylon->mY, flags, nullptr, 0, 0, 0);
            }
        }
        local[0] = 0.0f;
        local[1] = 0.0f;
        local[2] = 0.0f;
        sPylonRecord* pylon = NewPylon(0, 0, 7, 0, nullptr, reinterpret_cast<s3DPoint*>(local));
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, 0.0f + pylon->mY, 2, nullptr, 0, 0, 0);
    }
    SetPotentialPylonArea();
    FlashEndPylons(instance, 1);
    FlagGenerateTrack(instance, 1);
}
