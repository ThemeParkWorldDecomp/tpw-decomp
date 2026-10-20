// 0x1006f748 (1080 bytes): the skirt vertices of a ride's mesh (RideDestroy.cpp range).
#include <cstring>

#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"

namespace {

// Copies the deform's list into a block: a count word, the Mac offset of the data (block + 8), then the data.
void StoreNodeList(tag_sNode* node, int32_t count, const uint16_t* list) {
    uint32_t* block = static_cast<uint32_t*>(MReleaseAllocF(static_cast<uint32_t>(count) * 2 + 8, 0));
    node->mF98.mOffset = tpw::ArenaOffset(block);
    uint8_t* data = reinterpret_cast<uint8_t*>(block) + 8;
    block[1] = tpw::ArenaOffset(data);
    block[0] = static_cast<uint32_t>(count);
    if (data != nullptr && count != 0) std::memcpy(data, list, static_cast<size_t>(count) * 2);
}

}  // namespace

// The third argument is the ride's shape (null: no edges are decided), 4th and 5th its size. Unused local at 0x2d8 (a copy of
// 12 bytes of a global) is not kept.
// The list buffer is 256 entries (-0x288) with no bound: a longer list runs on into the matrix above it (-0x88, 64 bytes,
// dead once the first DoOrSetupSkirtDeform has its copy) and the unused copy. The jungle's incagod lists 275 entries.
void CalculateSkirtVertices(tag_sRideAnimSet* set, tag_sM3D2Header* header, void* a, int32_t w, int32_t h) {
    tag_sMesh* mesh = set->mHeads;
    tag_sM3D2Header* mh = mesh->mHeader;
    tag_sNode* bestNode = nullptr;
    int32_t bestCount = 0;
    int32_t count = 0;
    uint16_t buffer[256];
    uint16_t* list = nullptr;
    sMatrix matrix;

    if ((mh->mFlags & 0x80) == 0 && a != nullptr) {
        DecideRideShapeDeformEdges(static_cast<RideShape*>(a), w, h);
    } else {
        std::memset(gSkirtCellFlags, 0, 0x640);
        gSkirtColumns = 0;
        gSkirtRows = 0;
    }

    if (header != nullptr && header->mHField.mOffset != 0) {
        SetMatrixIdentity(&matrix);
        list = buffer;
        count = 0;
        bestNode = nullptr;
        bestCount = 0;
        uint32_t flags = (mh->mFlags & 0x100) ? 1 : 0;
        DoOrSetupSkirtDeform(mh->mF78.get(), mh->mF78.get(), &list, matrix, header->mHField.get(), 0, &count, 0.0f, &bestNode,
                             &bestCount, flags, nullptr, 0);
        if (bestNode != nullptr) bestNode->mFlags |= 0x4000;
        mesh->mRootNode = bestNode;
    }

    uint16_t countHalf = static_cast<uint16_t>(count);
    std::memcpy(mesh->mF0C, &countHalf, 2);
    if (count > 0) {
        mesh->mBlock14 = MReleaseAllocF(static_cast<uint32_t>(count) * 2, 1);
        if (mesh->mBlock14 != nullptr && count != 0) std::memcpy(mesh->mBlock14, buffer, static_cast<size_t>(count) * 2);
    }

    tag_sNode* node = mh->mNodes.get();
    for (uint32_t n = mh->mNodeCount; n != 0; --n, ++node) {
        if ((node->mFlags & 0x40) != 0 && node != mesh->mRootNode) {
            count = 0;
            list = buffer;
            SetMatrixIdentity(&matrix);
            DoOrSetupSkirtDeform(mh->mF78.get(), mh->mF78.get(), &list, matrix, header->mHField.get(), 0, &count, 0.0f, &bestNode,
                                 &bestCount, 0, node, 1);
            if (count > 0) StoreNodeList(node, count, buffer);
        }
    }

    if (a != nullptr) {
        uint32_t flags = mesh->mHeader->mFlags;
        if ((flags & 8) == 0) {
            ERRORBox("Ride %s has not been exported with 'has procedural base' selected\nFull reexport recommended\n",
                     mesh->mHeader->mName);
        } else if (mesh->mRootNode != nullptr || (flags & 0x80) != 0) {
            CalcSkirtTextureDirnFlags(set, static_cast<RideShape*>(a), w, h, static_cast<int32_t>(flags & 0x80));
        }
    }
}
