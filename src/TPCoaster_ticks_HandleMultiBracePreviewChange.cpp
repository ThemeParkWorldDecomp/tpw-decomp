#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// Clears the preview marks (0x1000000 and 0x4000) of the braces from lo up to, not including, hi, walking the prev links.
void ClearMarks(tag_sTrackBrace* lo, tag_sTrackBrace* hi) {
    for (; lo != hi; lo = lo->mPrev) lo->mFlags &= 0xfeffbfffu;
}

// The Mac keeps a brace in the word at 0x102c1788 (see DoDeleteMultiPylons): the host's pointer is kept in gCoasterStartBrace.
tag_sTrackBrace* Word1788Brace() {
    if (gCoasterWord1788 != 0 && gCoasterWord1788 == static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gCoasterStartBrace)))
        return gCoasterStartBrace;
    return nullptr;
}

}  // namespace

// 0x10054ea8: the pointed ride changed while placing a multi pylon selection (flag 0x80): marks the braces between the brace of the
// pointed ride and the first one; with flag 0x100 the pylon under the cursor is dragged, its cost shown and its position checked.
void HandleMultiBracePreviewChange(tag_sCoasterInstance* instance, tag_sRideAnimSet* previous, tag_sRideAnimSet* pointed, int32_t changed,
                                   uint32_t cellX, uint32_t cellZ) {
    (void)previous;
    uint32_t inputFlags = gCoasterInputFlags;
    if ((inputFlags & 0x80) != 0 && changed != 0) {
        tag_sTrackBrace* found = nullptr;
        if (pointed != nullptr) {
            tag_sCoasterInstance* edit = gCoasterEditInstance;
            if (edit->mLastBrace != nullptr) {
                for (sCoasterPylon* node = edit->mPylons; node != nullptr; node = node->mNext) {
                    if ((node->mFlags & 1) != 0) continue;
                    for (uint32_t i = 0; i < node->mCount; i++) {
                        tag_sTrackBrace* brace = node->mBraces[i];
                        if (gRideAnimSets[brace->mWord70] == pointed) {
                            found = brace;
                            goto searched;
                        }
                    }
                }
            }
        }
    searched:
        tag_sTrackBrace* a;   // r5, stored in gPylonMoveMark1
        tag_sTrackBrace* b;   // r3, the second brace
        if (found == nullptr) {
            a = Word1788Brace();
            b = found;
        } else {
            tag_sTrackBrace* held = Word1788Brace();
            if (found == held && (held->mData->mFlags & 2) != 0) {
                tag_sTrackBrace* mark2 = gPylonMoveMark2;
                if (mark2 != nullptr) {
                    tag_sTrackBrace* mark1 = gPylonMoveMark1;
                    if (mark1->mNumber > mark2->mNumber) ClearMarks(mark2, mark1);
                    else ClearMarks(mark1, mark2);
                }
                FlagGenerateTrack(gCoasterEditInstance, 0);
                return;
            }
            if (found == held) {
                tag_sTrackBrace* next = found->mNext;
                tag_sTrackBrace* prev = found->mPrev;
                uint32_t flags = found->mFlags;
                a = next;
                b = prev;
                if ((flags & 0x40) != 0) {
                    if ((flags & 0x80) != 0) {
                        b = prev->mPrev;
                        if (a == nullptr) a = found;
                        if (b == nullptr) b = prev;
                    } else {
                        a = a->mNext;
                        if (a == nullptr) a = next;
                        if (b == nullptr) b = found;
                    }
                } else {
                    if (a == nullptr) a = found;
                    if (b == nullptr) b = found;
                }
            } else {
                b = found;
                a = held;
                if (found->mNumber == 0) {
                    b = gCoasterEditInstance->mBraces;
                } else if (held->mNumber == 0) {
                    a = gCoasterEditInstance->mBraces;
                }
                if (a->mNumber > b->mNumber) {
                    tag_sTrackBrace* swap = a;
                    a = b;
                    b = swap;
                }
                // a is the lower, b the higher; the original reads a (r5) as the lower after its swap
                uint32_t flags = a->mFlags;
                if ((flags & 0x40) != 0 && (flags & 0x80) != 0 && a->mNext != nullptr) a = a->mNext;
                flags = b->mFlags;
                if ((flags & 0x40) != 0 && (flags & 0x80) == 0 && b->mPrev != nullptr) b = b->mPrev;
            }
        }
        tag_sTrackBrace* mark2 = gPylonMoveMark2;
        if (mark2 != nullptr) {
            tag_sTrackBrace* mark1 = gPylonMoveMark1;
            if (mark1->mNumber > mark2->mNumber) ClearMarks(mark2, mark1);
            else ClearMarks(mark1, mark2);
        }
        gPylonMoveMark1 = a;
        SetTrackPreview2ndBrace(b, 1);
        return;
    }
    if ((inputFlags & 0x100) == 0) return;

    const sTrackBraceData* data = gPylonMoveBrace->mData;
    if (data->mCellX != cellX || data->mCellZ != cellZ) {
        tag_sPylon* pylon = GetPylonUnderCursor(nullptr, nullptr, nullptr);
        FlagGenerateTrack(gCoasterEditInstance, 0);
        if (pylon != nullptr) {
            const sTrackBraceData* mine = gPylonMoveBrace->mData;
            if (pylon->mF18Table != mine->mF18Table || pylon->mF14 != mine->mF14) goto done;
            const sPylonControl* control = reinterpret_cast<const sPylonControl*>(pylon->mF18Table);
            if (pylon->mF30 >= static_cast<uint32_t>(control->mFCC)) goto done;
        }
        float a;
        float b;
        MoveBrace(instance, gPylonMoveBrace, pylon, cellX, cellZ, &a, &b, 1, 1);
        tag_sTrackBrace* brace = gPylonMoveBrace;
        if (gCoasterEditInstance != nullptr && gCoasterDragBrace != nullptr && gActionRecorder.mRecording == 0) {
            int32_t before = ppc_fctiwz(static_cast<double>(gCoasterCostBefore));
            int32_t total = before;
            // the original tests the address of gPylonSecondCost against NULL: always true
            total += ppc_fctiwz(static_cast<double>(gPylonSecondCost));
            int32_t after = RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, brace, &a, &b, nullptr, nullptr));
            after += RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, brace->mPrev, nullptr, nullptr, &a, &b));
            if (static_cast<uint32_t>(gPylonMoveX) == cellX && static_cast<uint32_t>(gPylonMoveZ) == cellZ) {
                gCoasterWord16BC = 0;
            } else {
                gCoasterWord16BC = static_cast<uint32_t>(after - total);
            }
        }
        if (IsValidPylonPosition(cellX, cellZ, nullptr, 0, 0, 0, 0) != 0) {
            if (gPylonMoveBrace != nullptr) gPylonMoveBrace->mFlags &= 0xffffbfffu;
            tag_sTrackBrace* next = gPylonMoveBrace->mNext;
            if (next != nullptr) next->mFlags &= 0xffffbfffu;
        } else {
            if (gPylonMoveBrace != nullptr) gPylonMoveBrace->mFlags |= 0x4000u;
            tag_sTrackBrace* next = gPylonMoveBrace->mNext;
            if (next != nullptr) next->mFlags |= 0x4000u;
        }
    }
done:
    if (gActionRecorder.mRecording == 0) {
        int32_t cost = static_cast<int32_t>(gCoasterWord16BC);
        if (cost < 0) LLCC_SystemAddToSessionCost(cost + (-0xeef0000) + 0x1112);
        else LLCC_SystemAddToSessionCost(cost);
    }
}
