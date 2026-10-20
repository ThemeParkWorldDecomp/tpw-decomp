// Unit v5893: RemoveTrackBraceFromPylon 0x10054640, DeletePylon 0x10054814, MoveBrace 0x10054978 (TPConfig.cpp; names invented).
// The pylon a brace points at is an sPylonRecord on the host (native pointers; the Mac offsets are in TPCoaster.h).
#include <cstdint>
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "ui_main.h"

namespace {

sPylonRecord* RecOf(const sTrackBraceData* data) { return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(data)); }

tag_sPylon* AsPylon(sPylonRecord* record) { return reinterpret_cast<tag_sPylon*>(record); }

sLinkPylon* AsLink(sPylonRecord* record) { return reinterpret_cast<sLinkPylon*>(record); }

// The brace's place in its pylon: the Mac word at +4.
uint32_t& PlaceOf(tag_sTrackBrace* brace) { return *reinterpret_cast<uint32_t*>(brace->mF00 + 4); }

// The float at +0x44 of the first node of the ride set's mesh header (the skirt height).
float& NodeHeight(uint32_t rideIndex) {
    tag_sRideAnimSet* set = gRideAnimSets[rideIndex];
    return set->mHeads->mHeader->mF78.get()->mMatrix.m[3][1];
}

const char kLinkLevelText[] = "Determining link level for brace %d being moved into other pylon\n";   // 0x1023d133

}  // namespace

void RemoveTrackBraceFromPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    sPylonRecord* pylon = RecOf(brace->mData);
    pylon->mCount = pylon->mCount - 1;
    uint32_t i = PlaceOf(brace);
    while (i < pylon->mCount) {
        if (i == PlaceOf(brace)) {
            float saved = NodeHeight(pylon->mBraces[i]->mWord70);
            float below;
            if (i == 0) {
                below = 0.0f;
            } else {
                float h = CalculateBraceHeight(pylon->mBraces[i - 1]);
                below = pylon->mControl->mFC4 + h;
            }
            tag_sTrackBrace* next = pylon->mBraces[i + 1];
            float nextHeight = CalculateBraceHeight(next);
            float diff = nextHeight - below;
            NodeHeight(next->mWord70) = saved;
            float interval = GetIntervalForHeight(diff, pylon->mControl);
            next->mFE8w = interval;
            if (next->mFE8w < 0.0f) next->mFE8w = 0.0f;   // fcmpo/bge and ble both branch past the store for a NaN
            if (next->mFE8w > 1.0f) next->mFE8w = 1.0f;
            SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(instance->mType)), next);
        }
        pylon->mBraces[i] = pylon->mBraces[i + 1];
        PlaceOf(pylon->mBraces[i]) -= 1;
        ++i;
    }
    pylon->mBraces[pylon->mCount] = nullptr;
}

void DeletePylon(tag_sCoasterInstance* instance, tag_sPylon* pylonArg, int32_t flag) {
    sPylonRecord* pylon = reinterpret_cast<sPylonRecord*>(pylonArg);
    if ((pylon->mFlags & 1) == 0 && flag == 0) Destroy3DObjectFromAIWorld(pylon->mCellX, pylon->mCellZ);
    if (pylon->mPrev != nullptr) pylon->mPrev->mNext = pylon->mNext;
    if (pylon->mNext != nullptr) pylon->mNext->mPrev = pylon->mPrev;
    if (reinterpret_cast<sPylonRecord*>(instance->mPylons) == pylon) instance->mPylons = reinterpret_cast<sCoasterPylon*>(pylon->mNext);
    if (instance->mPylonTail == pylon) instance->mPylonTail = pylon->mPrev;
    instance->mF40 = instance->mF40 - 1;
    sPylonRecord* node = pylon->mNext;
    if (node != nullptr) {
        uint32_t number = node->mPrev != nullptr ? node->mPrev->mNumber + 1 : 0;
        for (; node != nullptr; node = node->mNext) {
            node->mNumber = number;
            ++number;
        }
    }
    MReleaseFreeF(pylon);
    FlagGenerateTrack(instance, 1);
}

void MoveBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sPylon* pylonArg, uint32_t x, uint32_t z, float* outA, float* outB,
               int32_t c, int32_t d) {
    (void)c;   // r10 is never read
    sPylonRecord* orig = RecOf(brace->mData);
    if (orig->mCellX == x && orig->mCellZ == z) {
        if (d == 0 && instance != nullptr && orig != nullptr && orig->mControl != nullptr) {
            if (SquishPylonBracesIfRequired(instance, orig->mControl, AsLink(orig)) != 0) {
                UpdatePylonStackStructures(RecOf(brace->mData)->mControl, AsPylon(RecOf(brace->mData)));
                UpdatePylonStackMeshes(RecOf(brace->mData)->mControl, AsPylon(RecOf(brace->mData)), 0, 0);
            }
        }
    } else {
        sLinkPylon* pylon = reinterpret_cast<sLinkPylon*>(pylonArg);
        if (pylon == AsLink(orig)) pylon = nullptr;
        sPylonControl* control = orig->mControl;
        int32_t cx = static_cast<int32_t>(x);
        int32_t cz = static_cast<int32_t>(z);
        if (cx < 0) cx = 0;
        if (cz < 0) cz = 0;
        if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
        if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
        bool multi = orig->mCount > 1;
        if (multi) {
            RemoveTrackBraceFromPylon(instance, brace);
            if ((orig->mFlags & 2) == 0) Assign3DObjectToAIWorld(instance, orig->mCellX, orig->mCellZ, orig->mBraces[0]->mWord70, 0);
            UpdatePylonStackStructures(orig->mControl, AsPylon(orig));
            UpdatePylonStackMeshes(orig->mControl, AsPylon(orig), 0, 0);
        }
        sFixedRideParams* params = GetTempFixedRideParams(control->mFB4, control->mFB8, cx, cz);
        MoveRide(gRideAnimSets[brace->mWord70], cx, cz, params->mF0C, control->mFB4, control->mFB8, 0x3fa);
        uint32_t index = x + z * gHeightfieldHeader->mHField.get()->mF18[0];
        if (multi) {
            if (pylon != nullptr) {
                float height = gPylonMoveHeight;
                CTRACE(5, kLinkLevelText, brace->mNumber);
                uint32_t level;
                float out;
                if (DetermineLinkLevel(height, pylon, &level, &out, 0) != 0)
                    InsertBraceIntoPylon(instance, brace, pylon, level, out, nullptr, 0, 0, d, 0);
            } else {
                sPylonRecord* made = NewPylon(x, z, 0, 0, control, nullptr);
                InsertBraceIntoPylon(instance, brace, AsLink(made), 0, gPylonMoveHeight, nullptr, 0, 0, d, 0);
            }
        } else if (pylon != nullptr) {
            float height = gPylonMoveHeight;
            sPylonRecord* old = RecOf(brace->mData);
            RemoveTrackBraceFromPylon(instance, brace);
            DeletePylon(instance, AsPylon(old), 1);
            if (AsPylon(old) == gPylonMoveOrigData) gPylonMoveOrigData = nullptr;
            CTRACE(5, kLinkLevelText, brace->mNumber);
            uint32_t level;
            float out;
            if (DetermineLinkLevel(height, pylon, &level, &out, 0) != 0)
                InsertBraceIntoPylon(instance, brace, pylon, level, out, nullptr, 0, 0, d, 0);
        } else {
            RecOf(brace->mData)->mCellX = x;
            RecOf(brace->mData)->mCellZ = z;
            RecOf(brace->mData)->mIndex = index;
            RecOf(brace->mData)->mX = 10.0f * static_cast<float>(x);
            RecOf(brace->mData)->mZ = 10.0f * static_cast<float>(z);
            RecOf(brace->mData)->mY = NodeHeight(brace->mWord70);
        }
        FlagCollisionUpdateTrackAroundBrace(brace);
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        set->mHeads->mHeader->mFlags |= 0x4000;
        DoMeshSkirtDeform(set->mHeads, gHeightfieldHeader, RecOf(brace->mData)->mY, 0);
    }
    if (outA != nullptr) *outA = ppc_fmadds(10.0f, static_cast<float>(x), 5.0f);
    if (outB != nullptr) *outB = ppc_fmadds(10.0f, static_cast<float>(z), 5.0f);
}
