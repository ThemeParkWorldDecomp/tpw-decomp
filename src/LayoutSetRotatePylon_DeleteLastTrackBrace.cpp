// DeleteTrackBrace 0x10056660 and DeleteLastTrackBrace 0x10056a80 (unit v5337).
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

// Callees with no header yet.
void RemoveTrackBraceFromPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);   // 0x10054640
void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance);   // 0x10048fc4
void DeletePylon(tag_sCoasterInstance* instance, tag_sPylon* pylon, int32_t flag);   // 0x10054814

namespace {

// The first brace from `start` (inclusive) whose flag 0x40 is clear, or NULL.
tag_sTrackBrace* SkipMarked(tag_sTrackBrace* start) {
    for (tag_sTrackBrace* b = start; b != nullptr; b = b->mNext)
        if ((b->mFlags & 0x40) == 0) return b;
    return nullptr;
}

sPylonControl* ControlOf(const tag_sPylon* pylon) {
    return reinterpret_cast<sPylonControl*>(const_cast<float*>(pylon->mF18Table));
}

}  // namespace

// 0x10056660
void DeleteTrackBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    tag_sPylon* pylon = const_cast<tag_sPylon*>(brace->mData);
    RemoveTrackBraceFromPylon(instance, brace);
    if (instance->mFlashBrace == brace) {
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        if (set->mFlags & 0x4000000) StopFlashingObject(set);
        instance->mFlashBrace = nullptr;
    }
    if (instance->mBraceB5C == brace) instance->mBraceB5C = SkipMarked(brace->mNext);
    if (brace->mNext != nullptr) brace->mNext->mPrev = brace->mPrev;
    if (brace->mPrev != nullptr) brace->mPrev->mNext = brace->mNext;
    if (instance->mLastBrace == brace) instance->mLastBrace = brace->mPrev;
    if (instance->mBraces == brace) instance->mBraces = brace->mNext;
    // both arms of the original's test of flag 2 do the same
    if (brace->mNext != nullptr) instance->mPreviewBrace = brace->mNext;
    if (brace->mPrev != nullptr) instance->mBrace64 = brace->mPrev;

    tag_sTrackBrace* cur = (instance->mBlock3C & 2) ? instance->mPreviewBrace : instance->mBraces;
    if (cur != nullptr) {
        if (cur->mData->mFlags & 1) {
            instance->mF118 = reinterpret_cast<const float*>(instance->mType)[0xa4 / 4];
            if ((instance->mBlock3C & 2) == 0) instance->mBlock3C &= ~4u;
        } else {
            if (cur->mFlags & 0x40) cur = SkipMarked(cur->mNext);
            tag_sRideAnimSet* set = gRideAnimSets[cur->mWord70];
            int32_t index = ControlOf(cur->mData)->mF2C;
            const uint8_t* a = static_cast<const uint8_t*>(set->mHeads->mF28);
            const uint8_t* b;
            std::memcpy(&b, a + 4, sizeof(b));
            const uint8_t* c;
            std::memcpy(&c, b + index * 0x14 + 4, sizeof(c));
            float value = 0.0f;
            if (c != nullptr) std::memcpy(&value, c + 0x34, sizeof(value));
            instance->mF118 = value;
        }
    }
    instance->mBraceCount = instance->mBraceCount - 1;

    tag_sTrackBrace* p = brace->mPrev;
    if (p != nullptr) {
        int32_t n = (p->mNext != nullptr) ? static_cast<int32_t>(p->mNext->mNumber) + 1 : 0;
        for (; p != nullptr; p = p->mPrev) {
            p->mNumber = static_cast<uint32_t>(n);
            p->mF6C = static_cast<float>(static_cast<uint32_t>(n));
            n++;
        }
    }

    if (brace->mFlags & 0x40) {
        tag_sCoasterInstance* edit = gCoasterEditInstance;
        if ((brace->mFlags & 0x80) == 0) {
            if (edit != nullptr && (edit->mType[0] & 2)) {
                gCoasterInputFlags |= 0x40;
                gCoasterWord1790 = 1;
            }
        } else {
            if (edit != nullptr && (edit->mType[0] & 2)) {
                gCoasterInputFlags &= ~0x40u;
                gCoasterWord1790 = 0;
            }
        }
    }
    RemoveBraceFromCollisionSystem(brace, instance);
    if (gCoasterDragBrace == brace) gCoasterDragBrace = nullptr;
    DestroyTrackBrace(brace, ControlOf(pylon));
    if (pylon->mF30 == 0) {
        DeletePylon(instance, pylon, 0);
    } else {
        uint32_t ownerWord;
        std::memcpy(&ownerWord, reinterpret_cast<const uint8_t*>(pylon->mOwner) + 0x70, sizeof(ownerWord));
        Assign3DObjectToAIWorld(instance, pylon->mCellX, pylon->mCellZ, ownerWord, 0);
        UpdatePylonStackStructures(ControlOf(pylon), pylon);
        UpdatePylonStackMeshes(ControlOf(pylon), pylon, 0, 0);
    }
    SetPotentialPylonArea();
    FlagGenerateTrack(instance, 1);
}

namespace {

void RefreshButtons(bool setState, bool refresh) {
    if (gActionRecorder.mRecording != 0) return;
    if (setState) {
        GetCoasterEditAvailableButtons(nullptr);
        SetCoasterButtonState(1, 2);
        gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
        gUIInterface.ChangeCoasterButtonsDown(gCoasterEditDown);
    } else if (refresh) {
        GetCoasterEditAvailableButtons(nullptr);
        gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
        gUIInterface.ChangeCoasterButtonsDown(gCoasterEditDown);
    }
}

}  // namespace

// 0x10056a80
void DeleteLastTrackBrace() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    gActionRecorder.RecordAction(static_cast<RecordedAction>(0x41));
    gCoasterEditInstance->mFlags &= ~0x2000u;
    instance = gCoasterEditInstance;

    bool a = false;
    if (instance != nullptr && (instance->mType[0] & 2)) {
        if ((instance->mBlock3C & 1) == 0) {
            if (instance->mBraces->mFlags & 0x80) a = true;
        } else if (instance->mBlock3C & 2) {
            if (instance->mPreviewBrace->mFlags & 0x80) a = true;
        }
    }
    bool b = false;
    if (instance != nullptr && (instance->mType[0] & 2)) {
        if ((instance->mBlock3C & 1) == 0) {
            uint32_t f = instance->mBraces->mFlags;
            if ((f & 0x40) && !(f & 0x80)) b = true;
        } else if (instance->mBlock3C & 2) {
            uint32_t f = instance->mPreviewBrace->mFlags;
            if ((f & 0x40) && !(f & 0x80)) b = true;
        }
    }

    if (instance->mBlock3C & 2) {
        tag_sTrackBrace* brace = instance->mPreviewBrace;
        if (brace != nullptr && (brace->mData->mFlags & 2) == 0) {
            instance->mPreviewBrace = brace->mNext;
            DeleteTrackBrace(gCoasterEditInstance, brace);
            RefreshButtons(a, b);
            if (instance->mPreviewBrace != nullptr) instance->mPreviewBrace->mFlags |= 0x50000;
        }
    } else if ((instance->mBlock3C & 1) == 0) {
        tag_sTrackBrace* brace = instance->mBraces;
        if (brace != nullptr && (brace->mData->mFlags & 2) == 0) {
            DeleteTrackBrace(instance, brace);
            RefreshButtons(a, b);
        }
    }
}
