#include <cstdint>
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ActionRecorder.h"
#include "UIInterface.h"
#include "ui_main.h"

// DoDeleteMultiPylons 0x10053f98: the editor's delete button. With input flag 0x80 clear it starts a selection at the dragged brace; with it set
// it ends the selection, records it, deletes the braces between the two ends and fixes up the editor state.
// gCoasterWord1788 (0x102c1788) holds a brace on the Mac; the header declares it as a 32-bit word, too small for a host pointer, so the pointer
// lives in gCoasterStartBrace (defined here) and the word keeps its low 32 bits.
tag_sTrackBrace* gCoasterStartBrace;
// Not reproduced safely: a NULL gCoasterWord1788 with a recording, or no brace without flag 0x40 after the first (the original reads address 0).

namespace {

uint32_t Low32(const tag_sTrackBrace* brace) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(brace)); }
tag_sTrackBrace* Word1788() {
    if (gCoasterWord1788 != 0 && gCoasterWord1788 == Low32(gCoasterStartBrace)) return gCoasterStartBrace;
    return nullptr;
}
void SetWord1788(tag_sTrackBrace* brace) {
    gCoasterStartBrace = brace;
    gCoasterWord1788 = Low32(brace);
}

// The pylon height of the animation set the brace's ride type names: set->mHeads->mF28 (table) -> entries[index].mRec->mY, 0.0f without a record.
float BraceHeight(tag_sTrackBrace* brace) {
    tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
    const uint32_t* data = reinterpret_cast<const uint32_t*>(brace->mData->mF18Table);
    uint32_t index = data[0x2c / 4];
    sPylonHeadTable* table = static_cast<sPylonHeadTable*>(set->mHeads->mF28);
    sPylonHeadRec* rec = table->mEntries[index].mRec;
    if (rec != nullptr) return rec->mY;
    return 0.0f;
}

}  // namespace

void DoDeleteMultiPylons() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance == nullptr || instance->mF40 == 0) return;

    if ((gCoasterInputFlags & 0x80) == 0) {
        tag_sTrackBrace* drag = gCoasterDragBrace;
        SetWord1788(drag);
        if (drag != nullptr) {
            gCoasterInputFlags |= 0xa0;
            gPylonMoveMark1 = drag;
            gExtrudeTextureKind = 1;
        }
        return;
    }

    tag_sTrackBrace* hi = gCoasterDragBrace;
    if (hi == nullptr) return;
    tag_sTrackBrace* end = hi;
    bool record = true;
    bool wholeTrack = false;
    if (hi == Word1788() && (Word1788()->mData->mFlags & 2) != 0) record = false;

    bool flagged;
    if (gPylonMoveMark2 != nullptr) {
        tag_sTrackBrace* lo = gPylonMoveMark2;
        tag_sTrackBrace* top = gPylonMoveMark1;
        if (top->mNumber > lo->mNumber) {
            // lo stays Mark2, top stays Mark1
        } else {
            lo = gPylonMoveMark1;
            top = gPylonMoveMark2;
        }
        for (; lo != top; lo = lo->mPrev) lo->mFlags &= 0xfeffbfffu;
        flagged = true;
    } else {
        flagged = false;
    }
    gPylonMoveMark1 = nullptr;
    gPylonMoveMark2 = nullptr;
    gExtrudeTextureKind = 0;
    if (flagged) FlagGenerateTrack(gCoasterEditInstance, 0);

    if (!record) {
        SetWord1788(nullptr);
        gCoasterInputFlags &= 0xffffff7fu;
        return;
    }

    gActionRecorder.RecordAction(static_cast<RecordedAction>(0x3f), static_cast<uint16_t>(Word1788()->mNumber), static_cast<uint16_t>(end->mNumber));
    gCoasterEditInstance->mFlags &= 0xffffdfffu;
    if (Word1788()->mNumber == 0) {
        wholeTrack = true;
        SetWord1788(gCoasterEditInstance->mBraces);
    } else if (hi->mNumber == 0) {
        wholeTrack = true;
        end = gCoasterEditInstance->mBraces;
    }

    tag_sTrackBrace* first = Word1788();
    if (first->mNumber > end->mNumber) {
        SetWord1788(end);
        end = first;
    }
    first = Word1788();
    if (first->mPrev == end && (first->mFlags & 0x40) != 0 && (first->mFlags & 0x80) != 0 && (end->mFlags & 0x40) != 0) {
        if (first->mNext != nullptr) SetWord1788(first->mNext);
        if (end->mPrev != nullptr) end = end->mPrev;
    }
    if (Word1788() == end) {
        SetWord1788(Word1788()->mNext);
        end = end->mPrev;
    }
    first = Word1788();
    tag_sTrackBrace* last = end;
    tag_sTrackBrace* before = first->mPrev;
    tag_sTrackBrace* from = first;
    if (before != end) {
        SetWord1788(before);
        tag_sTrackBrace* nextOfEnd = end->mNext;
        uint32_t f = before->mFlags;
        if ((f & 0x40) != 0 && (f & 0x80) == 0) {
            SetWord1788(before->mNext);
            from = from->mNext;
        }
        end = nextOfEnd;
        if ((end->mFlags & 0x80) != 0) last = last->mPrev;
    }

    instance = gCoasterEditInstance;
    if (last == instance->mBraces) wholeTrack = true;
    instance->mBlock3C |= 2;
    if (from->mPrev == last) {
        instance->mPreviewBrace = from;
        instance->mBrace64 = last;
        from->mFlags |= 0x50000;
        last->mFlags |= 0xa0000;
        tag_sTrackBrace* walk = from;
        if ((from->mFlags & 0x40) != 0) {
            walk = from->mNext;
            while (walk != nullptr) {
                if ((walk->mFlags & 0x40) == 0) break;
                walk = walk->mNext;
            }
        }
        gCoasterEditInstance->mF118 = BraceHeight(walk);
    } else {
        from->mFlags |= 0x50000;
        last->mFlags |= 0xa0000;
        tag_sTrackBrace* brace = last->mNext;
        while (brace != from) {
            tag_sTrackBrace* next = brace->mNext;
            DeleteTrackBrace(instance, brace);
            brace = next;
        }
        if ((last->mData->mFlags & 1) != 0) instance->mF118 = reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mFA4;
    }

    if (wholeTrack) {
        from->mFlags &= 0xfffaffffu;
        gCoasterEditInstance->mBlock3C &= 0xfffffffcu;
        DestroyEndPylons(gCoasterEditInstance);
    }
    SetWord1788(nullptr);
    FlagGenerateTrack(gCoasterEditInstance, 1);
    gCoasterInputFlags &= 0xffffff5fu;

    bool selected = false;
    tag_sCoasterInstance* now = gCoasterEditInstance;
    if (now != nullptr && (*now->mType & 2) != 0) {
        if ((now->mBlock3C & 1) == 0) {
            if ((now->mBraces->mFlags & 0x80) != 0) selected = true;
        } else if ((now->mBlock3C & 2) != 0) {
            if ((now->mPreviewBrace->mFlags & 0x80) != 0) selected = true;
        }
    }

    DisableCurrentPylonEditState();
    gCoasterEditEnabled = 0;
    gCoasterEditDown = 0;
    SetCoasterButtonState(selected ? 2 : 1, 0);
    GetCoasterEditAvailableButtons(nullptr);
    gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    gUIInterface.ChangeCoasterButtonsDown(gCoasterEditDown);

    SetPotentialPylonArea();
    SetCoasterState(gCoasterEditInstance, 1);
    CoasterPlaceMouseMode();
}
