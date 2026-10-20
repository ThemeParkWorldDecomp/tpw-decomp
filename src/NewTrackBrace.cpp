// Unit v5157: the track editor's new brace and the undoing of its preview (TPCoaster.cpp; main app).
//   NewTrackBrace 0x10040304 (1,532 bytes), UnLinkPreviewBrace 0x1003fedc (1,000 bytes)
// The pylon records are sPylonRecords (native pointers, host offsets in the header); the instance's +0x3c block is its mBlock3C word.
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

namespace {

// 0x1029ccbc: the animation kind of each of the five time controls (flag bits 0x20 << i of the pylon control).
const uint32_t kAnimKinds[5] = {3, 10, 2, 9, 10};
// 0x1029ccd0: its index in the kind.
const uint32_t kAnimIndexes[5] = {0, 0, 0, 0, 1};

inline sPylonRecord* RecordOf(const sTrackBraceData* data) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(data));
}

}  // namespace

// 0x10040304: a new brace on the pylon; the three floats are the x, z and loft of a single-brace pylon (pylon flag 1). Flag bits: 1 restoring,
// 2 the second (preview) brace, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100 set brace flags, 0x200 assign the object to the AI world.
tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t flags, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra) {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    uint32_t* params = &instance->mBlock3C;
    // The Mac asks for 0x104 bytes; the host brace is larger (native pointers and the collision list), so ask for its size.
    tag_sTrackBrace* brace = static_cast<tag_sTrackBrace*>(MReleaseAllocF(sizeof(tag_sTrackBrace), 1));
    uint32_t second = flags & 2;
    if (second != 0) brace->mFlags |= 0x2000;
    if ((control == nullptr || (control->mFlags & 0x4000) == 0) && second == 0) instance->mBraceB5C = brace;
    if ((*params & 2) != 0) {
        instance->mPreviewBrace->mFlags &= 0xfffaffffu;
        brace->mFlags |= 0x50000;
        brace->mNext = instance->mPreviewBrace;
        brace->mPrev = instance->mPreviewBrace->mPrev;
        brace->mNext->mPrev = brace;
        brace->mPrev->mNext = brace;
        instance->mPreviewBrace = brace;
    } else if (second != 0) {
        instance->mBrace6C = brace;
    } else {
        if (instance->mLastBrace == nullptr) instance->mLastBrace = brace;
        if (instance->mBraces != nullptr) instance->mBraces->mPrev = brace;
        brace->mNext = instance->mBraces;
        instance->mBraces = brace;
        instance->mPreviewBrace = brace;
    }
    if (second == 0) {
        brace->mNumber = instance->mBraceCount;
        brace->mF6C = static_cast<float>(instance->mBraceCount);
    }
    if ((*params & 2) != 0) CorrectBraceNumbering(brace);
    if (control != nullptr && (control->mFlags & 0x4000) != 0) {
        if (loops == 0)
            brace->mFlags |= 0xe0;
        else
            brace->mFlags |= 0x60;
    }
    if ((pylon->mFlags & 1) != 0) {
        brace->mX4C = x;
        brace->mZ54 = z;
        brace->mL50 = loft;
        brace->mCount78 = 1;
        brace->mBlk7C = reinterpret_cast<char*>(brace) + 0xc;
        brace->mBlk80 = MReleaseAllocF(0x58, 1);
        brace->mCount84 = 1;
        brace->mBlk88 = brace->mBlk7C;
        brace->mBlk8C = brace->mBlk80;
        brace->mF90 = reinterpret_cast<sBraceSpline*>(&brace->mCount78);
        // The first info's two blocks each hold the brace at +0x10.
        *reinterpret_cast<tag_sTrackBrace**>(static_cast<char*>(brace->mBlk7C) + 0x10) = brace;
        *reinterpret_cast<tag_sTrackBrace**>(static_cast<char*>(brace->mBlk80) + 0x10) = brace;
    } else {
        if (control != nullptr && (control->mFlags & 0x4000) != 0 && loops != 0)
            brace->mFF8 = 0.5f;
        else
            brace->mFF8 = 0.0f;
        sFixedRideParams* ride = GetTempFixedRideParams(control->mFB4, control->mFB8, static_cast<int32_t>(pylon->mCellX),
                                                        static_cast<int32_t>(pylon->mCellZ));
        if ((flags & 1) == 0) {
            brace->mWord70 = InstanceRide(control->mRideType, 0x3fa, ride, 0, 0, nullptr, 0, 0, 0xffffffffu, 0);
        } else {
            brace->mWord70 = RestoreRideInstance(extra, control->mRideType, 0x3fa, ride, 0, nullptr, 0, 0, 0);
        }
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        set->mFlags |= 0x80000;
        set->mBraceBlock = brace;
        set = gRideAnimSets[brace->mWord70];
        char* timeControl = static_cast<char*>(set->mTimeControls);
        for (uint32_t i = 0; i < 5; i++) {
            if ((control->mFlags & (0x20u << i)) != 0) {
                uint32_t kind = kAnimKinds[i];
                if ((set->mGroups[kind].mFlags & 3) != 0)
                    SetNewAnim(set, kind, kAnimIndexes[i], reinterpret_cast<sAnimTimeControl*>(timeControl), 0, 1.0f);
            }
            timeControl += sizeof(sAnimTimeControl);
        }
        set->mFlags |= 0x1000;
        float value = brace->mFE8w * control->mFD0[2];
        value = value + control->mFD0[3];
        value = value + 0.0f;
        if (value < control->mFD0[0]) value = control->mFD0[0];
        if (control->mFD0[1] < value) value = control->mFD0[1];
        sAnimTimeControl* first = static_cast<sAnimTimeControl*>(gRideAnimSets[brace->mWord70]->mTimeControls);
        first->mF20 = value * first->mF1C;
        if ((control->mFlags & 8) != 0) {
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount78), reinterpret_cast<sBraceControl*>(&control->mF30), control);
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount84), reinterpret_cast<sBraceControl*>(&control->mF44), control);
        } else {
            SetupBraceInfo(brace, reinterpret_cast<sBraceInfo*>(&brace->mCount78), reinterpret_cast<sBraceControl*>(&control->mF30), control);
        }
        if ((flags & 6) == 0) *params |= 4;
    }
    if ((flags & 8) != 0) brace->mFlags |= 0x200;
    uint32_t bit10 = flags & 0x10;
    if (bit10 != 0) brace->mFlags |= 0x400;
    if ((flags & 0x20) != 0) brace->mFlags |= 0x1800;
    if ((flags & 0x40) != 0) brace->mFlags |= 0x80000;
    if ((flags & 0x80) != 0) brace->mFlags |= 0x40000;
    if ((flags & 0x100) != 0) brace->mFlags |= 0x100000;
    if ((flags & 0x200) != 0 && (flags & 1) == 0)
        Assign3DObjectToAIWorld(gCoasterEditInstance->mCObject, pylon->mCellX, pylon->mCellZ, brace->mWord70, static_cast<int32_t>(bit10));
    if (second == 0) {
        uint32_t four = flags & 4;
        InsertBraceIntoPylon(gCoasterEditInstance, brace, reinterpret_cast<sLinkPylon*>(pylon), level, loft, nullptr, 0,
                             static_cast<int32_t>(flags & 1), 0, static_cast<int32_t>(four));
        if (four == 0) {
            sPylonControl* pylonControl = pylon->mControl;
            if (pylonControl != nullptr && (pylonControl->mFlags & 0x4000) == 0) gCoasterEditInstance->mF118 = loft;
        }
        instance->mBraceCount = instance->mBraceCount + 1;
        if (instance->mBraceCount >= 2) instance->mF48 = instance->mF48 + 1.0f;
    }
    if ((flags & 1) == 0) FlagCollisionUpdateTrackAroundBrace(brace);
    return brace;
}

namespace {

// The common end of the three ways that take the brace out of the pylon: the brace is unlinked from the track, its pylon loses it, the
// pylon's braces are re-pointed and the stack meshes rebuilt, the brace is cleared and the next brace renumbered.
void UnLinkEnd(tag_sCoasterInstance* instance, tag_sTrackBrace* next) {
    UnLinkBrace(instance->mBrace6C, reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C));
    tag_sTrackBrace* brace = instance->mBrace6C;
    sPylonRecord* pylon = RecordOf(brace->mData);
    if (pylon != nullptr) {
        if (pylon->mCount < 2) {
            // The Mac clears the word at pylon + 0x30 + count * 4: the last brace slot, or the count itself when it is 0.
            if (pylon->mCount == 0)
                pylon->mCount = 0;
            else
                pylon->mBraces[0] = nullptr;
            pylon->mCount = pylon->mCount - 1;
        } else {
            RestorePylonDetails(instance, reinterpret_cast<tag_sPylon*>(pylon));
        }
        if (gPylonStackSkip == 0) {
            for (uint32_t i = 0; i < pylon->mCount; i++) {
                tag_sTrackBrace* member = pylon->mBraces[i];
                uint32_t count = pylon->mCount;
                if (count > 1 && i < count - 1)
                    member->mFlags |= 0x100;
                else
                    member->mFlags &= 0xfffffeffu;
                sPylonRecord* memberPylon = RecordOf(member->mData);
                if ((member->mFlags & 0x2000) == 0 && (member->mFlags & 0x100) != 0 && (memberPylon->mControl->mFlags & 8) != 0) {
                    member->mF90 = reinterpret_cast<sBraceSpline*>(&member->mCount84);
                    member->mF74 = reinterpret_cast<const uint32_t*>(&memberPylon->mControl->mF44);
                } else {
                    member->mF90 = reinterpret_cast<sBraceSpline*>(&member->mCount78);
                    member->mF74 = reinterpret_cast<const uint32_t*>(&memberPylon->mControl->mF30);
                }
            }
        }
        UpdatePylonStackMeshes(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon), 0, 0);
    }
    brace->mFlags &= 0xffffff3fu;
    brace->mData = nullptr;
    CorrectBraceNumbering(next);
}

}  // namespace

// 0x1003fedc: takes the editor's preview brace (mBrace6C) back out of its pylon and the track.
void UnLinkPreviewBrace(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* next = instance->mBrace6C->mNext;
    if (gLinkFoundPylon == nullptr) {
        if ((instance->mBrace6C->mFlags & 0x8000) != 0) {
            RemoveBraceFromCollisionSystem(instance->mBrace6C, instance);
            FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        }
        sPylonRecord* preview = instance->mPreviewPylon;
        if ((preview->mFlags & 8) != 0) {
            if (preview->mPrev != nullptr) preview->mPrev->mNext = nullptr;
            instance->mPylonTail = preview->mPrev;
            preview->mPrev = nullptr;
            preview->mNext = nullptr;
            preview->mNumber = 0;
            instance->mF40 = instance->mF40 - 1;
            preview->mFlags &= 0xfffffff7u;
        }
        UnLinkEnd(instance, next);
    } else if ((instance->mFlags & 8) == 0) {
        if ((instance->mBrace6C->mFlags & 0x8000) != 0) {
            RemoveBraceFromCollisionSystem(instance->mBrace6C, instance);
            FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        }
        UnLinkEnd(instance, next);
    } else {
        instance->mFlags &= 0xfffffff7u;
        instance->mPreviewBrace->mFlags |= 0x50000;
        instance->mBrace64->mFlags &= 0xff7fbfffu;
        instance->mBrace64->mFlags |= 0xa0000;
        FlagCollisionUpdateTrackAroundBrace(instance->mPreviewBrace);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace64);
    }
}
