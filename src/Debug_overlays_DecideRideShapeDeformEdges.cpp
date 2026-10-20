#include "Debug.h"
#include <cmath>
#include <cstring>
#include "CBalanceTable.h"
#include "LevelSystems.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

// 0x1006d0f8: ends the highlight of the ride gHighlightedRide (only when it has flag 0x20000): every node that is not flagged 0x4000
// gets its flash flag (0x1000000) cleared unless the ride is being flashed (0x4000000), and its points pointer is restored from the
// table of saved colours. Then the ride's flag 0x20000 and the header's 0x400000 are cleared and the global is emptied.
void UnhighlightRide() {
    if (gHighlightedRide == nullptr) return;
    if ((gHighlightedRide->mFlags & 0x20000) == 0) return;
    tag_sM3D2Header* header = gHighlightedRide->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    for (uint32_t i = 0; i < header->mNodeCount; ++i) {
        if ((node->mFlags & 0x4000) == 0) {
            if ((gHighlightedRide->mFlags & 0x4000000) == 0) node->mFlags &= 0xfeffffffu;
            node->mPoints.mOffset = static_cast<const uint32_t*>(gHighlightNodeColours)[i];
        }
        ++node;
    }
    gHighlightedRide->mFlags &= 0xfffdffffu;
    gHighlightedRide->mHeads->mHeader->mFlags &= 0xffbfffffu;
    gHighlightedRide = nullptr;
}

// 0x1006d2f8: pushes the set on the list of flashing sets and flags it. A set that still shares its points with the original
// header gets its own copy of the point block (header flag 0x100000) and every node points into it.
void StartFlashingObject(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    set->mFlags |= 0x4000000u;
    header->mFlags |= 0x800000u;
    set->mFD0 = gFlashObjects;
    gFlashObjects = set;
    tag_sM3D2Header* original = header->mSelf.get();
    if (header->mPoints.mOffset == original->mPoints.mOffset) {
        header->mFlags |= 0x100000u;
        void* block = MReleaseAllocF(static_cast<uint32_t>(header->mPointCount) * 12u, 1);
        header->mPoints = static_cast<s3DPoint*>(block);
        tag_sNode* node = header->mNodes.get();
        uint32_t cursor = header->mPoints.mOffset;
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mPoints.mOffset = cursor;
            node->mFlags |= 0x1000000u;
            cursor += (static_cast<uint32_t>(node->mVertexCount) + static_cast<uint32_t>(node->mF5C)) * 12u;
            ++node;
        }
    } else {
        tag_sNode* node = header->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mFlags |= 0x1000000u;
            ++node;
        }
    }
}

// 0x1006d430: takes the set off the list (it must be in it) and gives its nodes back their original points: the copied block is
// freed and the pointers taken from the original header, or else the copied points are overwritten with the original's.
// A highlighted set is unhighlighted first and highlighted again at the end.
void StopFlashingObject(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    tag_sM3D2Header* original = header->mSelf.get();
    set->mFlags &= 0xfbffffffu;
    header->mFlags &= 0xff7fffffu;
    if (set == gFlashObjects) {
        gFlashObjects = set->mFD0;
    } else {
        tag_sRideAnimSet* previous = gFlashObjects;
        while (previous->mFD0 != set) previous = previous->mFD0;
        previous->mFD0 = set->mFD0;
    }
    set->mFD0 = nullptr;
    bool highlighted = (set->mFlags & 0x20000) != 0;
    if (highlighted) UnhighlightRide();
    if ((header->mFlags & 0x100000) != 0) {
        header->mFlags &= 0xffefffffu;
        MReleaseFreeF(header->mPoints.get());
        header->mPoints.mOffset = original->mPoints.mOffset;
        tag_sNode* node = header->mNodes.get();
        tag_sNode* source = original->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mPoints.mOffset = source->mPoints.mOffset;
            node->mFlags &= 0xfeffffffu;
            ++source;
            ++node;
        }
    } else {
        tag_sNode* node = header->mNodes.get();
        tag_sNode* source = original->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mFlags &= 0xfeffffffu;
            if (node->mPoints.get() != nullptr && source->mPoints.get() != nullptr && node->mF5E != 0)
                std::memcpy(node->mPoints.get(), source->mPoints.get(), static_cast<size_t>(node->mF5E) * 12u);
            ++source;
            ++node;
        }
    }
    if (highlighted) HighlightRide();
}

// 0x1006d76c: marks the edges of a ride shape (width columns, height rows) for the skirt: a cell is solid when its value has a bit of
// 0xf; a solid and an empty neighbour (outside the shape counts as empty) set the bit of that side in gSkirtCellFlags: 4 below,
// 2 left, 1 above, 8 right.
void DecideRideShapeDeformEdges(RideShape* shape, int32_t width, int32_t height) {
    std::memset(gSkirtCellFlags, 0, 0x640);
    gSkirtColumns = static_cast<uint32_t>(width);
    gSkirtRows = static_cast<uint32_t>(height);
    auto solid = [&](int32_t row, int32_t col) {
        uint32_t v = static_cast<uint32_t>(shape->mCells[row][col].mValue);
        return (v & 4) != 0 || (v & 9) != 0 || (v & 0xa) != 0;
    };
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            bool self = solid(row, col);
            bool other = row < height - 1 ? solid(row + 1, col) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 4;
            other = col > 0 ? solid(row, col - 1) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 2;
            other = row > 0 ? solid(row - 1, col) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 1;
            other = col < width - 1 ? solid(row, col + 1) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 8;
        }
    }
}
