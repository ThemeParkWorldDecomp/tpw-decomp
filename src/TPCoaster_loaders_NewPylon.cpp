// TPCoaster.cpp (main app), unit v5091: NewPylon (0x1003d610) and UpdatePylonStackMeshes (0x1003d958).
#include <cstddef>
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"

uint32_t gPylonStackSkip = 0;   // 0x102b68a0

// 0x1003d610: a pylon record, linked into the editor coaster's list (or made its preview pylon when flag 4 is set), placed by a point or by a
// cell. The fourth argument is never read. The Mac asks MReleaseAllocF for 0x48 bytes; the host record is bigger (native pointers), so it asks
// for sizeof.
sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    (void)unused;
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    sPylonRecord* pylon = static_cast<sPylonRecord*>(MReleaseAllocF(sizeof(sPylonRecord), 1));
    if ((flags & 4) != 0) {
        instance->mPreviewPylon = pylon;
    } else {
        if (instance->mPylons == nullptr) instance->mPylons = reinterpret_cast<sCoasterPylon*>(pylon);
        if (instance->mPylonTail != nullptr) instance->mPylonTail->mNext = pylon;
        pylon->mPrev = instance->mPylonTail;
        instance->mPylonTail = pylon;
        pylon->mNumber = instance->mF40;
        instance->mF40 = instance->mF40 + 1;
    }
    pylon->mFlags = flags;
    if (point != nullptr) {
        pylon->mX = point->x;
        pylon->mY = point->y;
        pylon->mZ = point->z;
        const float ten = 10.0f;
        pylon->mCellX = static_cast<uint32_t>(FloatToCell(point->x / ten));
        pylon->mCellZ = static_cast<uint32_t>(FloatToCell(point->z / ten));
        if (static_cast<int32_t>(pylon->mCellX) < 0) pylon->mCellX = 0;
        if (static_cast<int32_t>(pylon->mCellZ) < 0) pylon->mCellZ = 0;
        if (static_cast<int32_t>(pylon->mCellX) >= gHeightfieldWidth) pylon->mCellX = static_cast<uint32_t>(gHeightfieldWidth - 1);
        if (static_cast<int32_t>(pylon->mCellZ) >= gHeightfieldHeight) pylon->mCellZ = static_cast<uint32_t>(gHeightfieldHeight - 1);
    } else {
        pylon->mCellX = x;
        pylon->mCellZ = z;
        const float ten = 10.0f;
        pylon->mX = ten * static_cast<float>(x);
        pylon->mZ = ten * static_cast<float>(z);
        pylon->mY = GetRideRequiredYOffset(static_cast<int32_t>(x), static_cast<int32_t>(z), nullptr, 1, 1, 0, 0);
        pylon->mIndex = x + z * gHeightfieldHeader->mHField->mF18[0];
    }
    pylon->mInstance = gCoasterEditInstance;
    pylon->mControl = control;
    return pylon;
}

namespace {

// The node (0xa0 bytes) or mini node (0x58) of the ride's mesh with the index the pylon control gives, past the mesh's first node.
uint32_t* StackNodeWord(tag_sRideAnimSet* set, uint32_t index) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t first = header->mF46;
    uint32_t count = header->mNodeCount;
    uint32_t i = index + first;
    if (i < count) return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + i * 0xa0);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) + (i - count) * 0x58);
}

void SetStackNode(tag_sRideAnimSet* set, uint32_t index) { *StackNodeWord(set, index) |= 0x80000010u; }
void ClearStackNode(tag_sRideAnimSet* set, uint32_t index) { *StackNodeWord(set, index) &= ~0x10u; }

}  // namespace

// 0x1003d958 (the pylon is a sLinkPylon / sPylonRecord; tag_sPylon is the typedef the header declared it with).
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylonArg, int32_t a, int32_t b) {
    sLinkPylon* pylon = reinterpret_cast<sLinkPylon*>(pylonArg);
    if (gPylonStackSkip != 0 && a == 0) return;
    tag_sTrackBrace** braces = pylon->mBraces;
    for (uint32_t i = 0; i < pylon->mCount; i++) {
        tag_sTrackBrace* brace = braces[i];
        if (b == 0) FlagCollisionUpdateTrackAroundBrace(brace);
        if ((brace->mFlags & 0x2000) != 0) continue;
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        if (i == 0) {
            float y = pylon->mY;
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader->mF78.get()) + 0x44) = y;
            AnimateRideSet(set, 1, 0);
            uint32_t* root = reinterpret_cast<uint32_t*>(set->mHeads->mRootNode);
            if (root != nullptr) {
                *root &= ~0x10u;
                const float skirt = *reinterpret_cast<const float*>(brace->mData->mF1C + 4);
                DoMeshSkirtDeform(set->mHeads, gHeightfieldHeader, skirt, 0);
            }
        } else {
            tag_sTrackBrace* before = braces[i - 1];
            float y;
            if ((before->mFlags & 0x2000) != 0) {
                y = *reinterpret_cast<const float*>(before->mF0C + (0x50 - 0x0c)) + pylon->mControl->mFC4;
            } else {
                tag_sRideAnimSet* beforeSet = gRideAnimSets[before->mWord70];
                sPylonHeadTable* table = static_cast<sPylonHeadTable*>(beforeSet->mHeads->mF28);
                sPylonHeadRec* rec = table->mEntries[control->mF18].mRec;
                y = rec != nullptr ? rec->mY : 0.0f;
            }
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader->mF78.get()) + 0x44) = y;
            AnimateRideSet(set, 1, 0);
            if ((control->mFlags & 0x10) == 0) {
                uint32_t* root = reinterpret_cast<uint32_t*>(set->mHeads->mRootNode);
                if (root != nullptr) *root |= 0x80000010u;
            }
        }
        if (i == pylon->mCount - 1) {
            if ((control->mFlags & 0x400) != 0) SetStackNode(set, control->mF1C);
            if ((control->mFlags & 0x800) != 0) ClearStackNode(set, control->mF20);
        } else {
            if ((control->mFlags & 0x400) != 0 && (braces[i + 1]->mFlags & 0x2000) == 0) ClearStackNode(set, control->mF1C);
            if ((control->mFlags & 0x800) != 0) SetStackNode(set, control->mF20);
        }
        if (i == 0) {
            if ((control->mFlags & 0x1000) != 0) SetStackNode(set, control->mF24);
            if ((control->mFlags & 0x2000) != 0) ClearStackNode(set, control->mF28);
        } else {
            if ((control->mFlags & 0x1000) != 0) ClearStackNode(set, control->mF24);
            if ((control->mFlags & 0x2000) != 0) SetStackNode(set, control->mF28);
        }
    }
}
