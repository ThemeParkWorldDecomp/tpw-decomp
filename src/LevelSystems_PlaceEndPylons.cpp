// PlaceEndPylons 0x100424f0 (508 bytes) and CoasterUVAnimTick 0x100428d4 (676 bytes), TPCoaster.cpp.
#include <cmath>
#include <cstdio>
#include <cstring>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "m3d2.h"
#include "tb_matrix.h"
#include "platform/samsutils.h"

// 0x100424f0 (508 bytes). The height argument is never read. The end pylons (one with the type's flag 0x20, else two) take the position of a
// head block of a ride's embedded data; when the entry is not found the point keeps what it had (the Mac leaves the stack slot as it was:
// uninitialised on the first round, the previous round's value on the second).
void PlaceEndPylons(tag_sCoasterInstance* instance, float height) {
    (void)height;
    sPylonRecord* first = reinterpret_cast<sPylonRecord*>(instance->mPylons);
    const uint32_t* type = instance->mType;
    FloatToCell(first->mX);
    FloatToCell(first->mZ);
    const uint32_t count = (type[0] & 0x20) != 0 ? 1 : 2;
    s3DPoint point = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < count; i++) {
        tag_sRideAnimSet* set = nullptr;
        uint32_t entry;
        if ((type[0] & 0x20) != 0) {
            tag_sTrackBrace* brace = instance->mLastBrace;
            const uint8_t* block = reinterpret_cast<const uint8_t*>(brace->mData->mF18Table);
            std::memcpy(&entry, block + 0x2c, 4);
            set = gRideAnimSets[brace->mWord70];
        } else {
            set = gRideAnimSets[instance->mEntranceID];
            entry = FindDataNodeIndexByID(set, 0x400000, i + 1);
        }
        if (entry != 0xffffffffu) {
            sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
            const uint8_t* block = embedded->mRecords[entry].mBlock;
            if (block != nullptr) {
                std::memcpy(&point.x, block + 0x30, 4);
                std::memcpy(&point.y, block + 0x34, 4);
                std::memcpy(&point.z, block + 0x38, 4);
            } else {
                point.x = point.y = point.z = 0.0f;
            }
        }
        const float zero = 0.0f;
        sPylonRecord* pylon = NewPylon(0, 0, 0x13, 0, nullptr, &point);
        const uint32_t flags = (i == 0 && (type[0] & 0x20) != 0) ? 0x144 : 4;
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, zero + pylon->mY, flags, nullptr, 0, 0, 0);
    }
}

// 0x100428d4 (676 bytes). Scrolls every dynamic texture's offset by the elapsed time (the difference of two unsigned times, in thousandths
// of a second, times the coaster's speed word) and then moves the V coordinates of the vertices that the track mesh sets' flagged face sets
// use back by the same step. The step is reduced by every whole number an offset gives back (kept).
void CoasterUVAnimTick(tag_sCoasterInstance* instance) {
    const uint32_t now = GetRideAnimTime(0);
    uint32_t last;
    std::memcpy(&last, instance->mF138, 4);
    float step = instance->mF2C * (static_cast<float>(static_cast<double>(now - last)) / 1000.0f);
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    float* offset = instance->mUVOffsets;
    for (uint32_t n = type->mTextureCount; n != 0; n--) {
        *offset = *offset + step;
        if (*offset > 1.0f) {
            double whole;
            std::modf(static_cast<double>(*offset), &whole);
            const float cut = static_cast<float>(whole);
            *offset = *offset - cut;
            step = step - cut;
        }
        offset++;
    }
    sDTextureSet* textures = type->mTextures;
    for (tag_sDMeshSet* set = instance->mF38; set != nullptr; set = set->mF38) {
        int32_t used[256];
        std::memset(used, 0, 0x400);
        uint32_t maxIndex = 0;
        tag_sNode* node = set->mMesh->mHeader->mF78.get();
        const sFaceSet* faceSet = node->mFaceSets.get();
        for (uint32_t i = 0; i < node->mFaceSetCount; i++, faceSet++) {
            const int32_t bytes = static_cast<int32_t>(reinterpret_cast<intptr_t>(faceSet->mTexture.get()) -
                                                       reinterpret_cast<intptr_t>(textures->mTextures));
            const int32_t slot = bytes / 8;
            if ((textures->mEntries[slot].mFlags & 0x20) == 0) continue;
            const sFace* face = node->mFaces.get() + faceSet->mFirstFace;
            for (uint32_t k = faceSet->mFaceCount; k != 0; k--, face++) {
                used[face->mF02[0]] = 1;
                used[face->mF02[1]] = 1;
                used[face->mF02[2]] = 1;
                if (face->mF02[0] > maxIndex) maxIndex = face->mF02[0];
                if (face->mF02[1] > maxIndex) maxIndex = face->mF02[1];
                if (face->mF02[2] > maxIndex) maxIndex = face->mF02[2];
            }
        }
        sUVSet* uvs = node->mUVs.get();
        for (uint32_t k = 0; k <= maxIndex; k++) {
            if (used[k] == 0) continue;
            sUVSet& block = uvs[k >> 2];
            const float u = block.mF00[k & 3];
            const float v = block.mF00[4 + (k & 3)];
            block.mF00[k & 3] = u;
            block.mF00[4 + (k & 3)] = v - step;
        }
    }
    std::memcpy(instance->mF138, &now, 4);
}
