// Unit v5084: the pylon edit states of the track editor (TPConfig.cpp; names invented):
//   ExchangeTrackBraces 0x10053594, ToggleSwapUpPylonState 0x10053720, ToggleSwapDownPylonState 0x10053898,
//   SetTrackPreview2ndBrace 0x10053a04, DisableCurrentPylonEditState 0x10053b58
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"

uint32_t gCoasterWord1788 = 0;   // 0x102c1788
uint32_t gCoasterWord1790 = 0;   // 0x102c1790
uint32_t gCoasterWord16DC = 0;   // 0x102c16dc

namespace {

// The brace's word at +4 (inside the bytes mF00 on the host): its place in the pylon's list.
uint32_t IndexOf(const tag_sTrackBrace* brace) {
    uint32_t value;
    std::memcpy(&value, brace->mF00 + 4, sizeof(value));
    return value;
}

void SetIndex(tag_sTrackBrace* brace, uint32_t value) { std::memcpy(brace->mF00 + 4, &value, sizeof(value)); }

sCoasterPylon* PylonOf(const tag_sTrackBrace* brace) {
    return reinterpret_cast<sCoasterPylon*>(const_cast<sTrackBraceData*>(brace->mData));
}

// The float at +0x44 of the first node of the ride set's mesh (the node block is in the file's layout).
float* NodeFloat(tag_sRideAnimSet* set) {
    uint8_t* node = reinterpret_cast<uint8_t*>(set->mHeads->mHeader->mF78.get());
    return reinterpret_cast<float*>(node + 0x44);
}

}  // namespace

// 0x10053594: the two braces of a pylon trade places (their numbers, the pylon's slots, a ride set float and the loft).
void ExchangeTrackBraces(tag_sTrackBrace* first, tag_sTrackBrace* second) {
    sCoasterPylon* pylon = PylonOf(first);
    uint32_t firstIndex = IndexOf(first);
    SetIndex(first, IndexOf(second));
    SetIndex(second, firstIndex);
    tag_sTrackBrace** slots = &pylon->mBraces[0];
    slots[IndexOf(first)] = first;
    slots[IndexOf(second)] = second;
    tag_sRideAnimSet* setA = gRideAnimSets[first->mWord70];
    tag_sRideAnimSet* setB = gRideAnimSets[second->mWord70];
    float* a = NodeFloat(setA);
    float held = *a;
    *a = *NodeFloat(setB);
    *NodeFloat(setB) = held;
    float loft = first->mFE8w;
    first->mFE8w = second->mFE8w;
    second->mFE8w = loft;
    SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), first);
    SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), second);
    UpdatePylonStackStructures(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon));
    FlagGenerateTrack(gCoasterEditInstance, 1);
    UpdatePylonStackMeshes(pylon->mControl, reinterpret_cast<tag_sPylon*>(pylon), 0, 0);
}

// 0x10053720: the swap-up drag starts or ends; a started one swaps the dragged brace with the next of its pylon.
void ToggleSwapUpPylonState() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance == nullptr || instance->mF40 == 0) return;
    uint32_t flags = gCoasterInputFlags;
    tag_sTrackBrace* brace = gCoasterDragBrace;
    const uint32_t bit = flags & 8;
    if (bit == 0) {
        if (brace == nullptr) return;
        if (!(IndexOf(brace) < PylonOf(brace)->mCount - 1)) return;
    }
    if ((brace->mData->mFlags & 2) != 0) return;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 8;
    } else if (bit != 0) {
        gCoasterInputFlags &= ~8u;
    }
    if ((gCoasterInputFlags & 8) != 0) {
        if (brace != nullptr) {
            gActionRecorder.RecordAction(0x3d, static_cast<uint16_t>(gCoasterDragBrace->mNumber));
            gCoasterEditInstance->mFlags &= 0xffffdfffu;
            tag_sTrackBrace* drag = gCoasterDragBrace;
            ExchangeTrackBraces(drag, PylonOf(drag)->mBraces[IndexOf(drag) + 1]);
            LockPointedAtRide(1);
            ToggleSwapUpPylonState();
            return;
        }
        gCoasterInputFlags &= ~8u;
    }
    LockPointedAtRide(0);
}

// 0x10053898: the same downwards (the previous brace of the pylon).
void ToggleSwapDownPylonState() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance == nullptr || instance->mF40 == 0) return;
    uint32_t flags = gCoasterInputFlags;
    tag_sTrackBrace* brace = gCoasterDragBrace;
    const uint32_t bit = flags & 0x10;
    if (bit == 0) {
        if (brace == nullptr) return;
        if (IndexOf(brace) == 0) return;
    }
    if ((brace->mData->mFlags & 2) != 0) return;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 0x10;
    } else if (bit != 0) {
        gCoasterInputFlags &= ~0x10u;
    }
    if ((gCoasterInputFlags & 0x10) != 0) {
        if (brace != nullptr) {
            gActionRecorder.RecordAction(0x3e, static_cast<uint16_t>(gCoasterDragBrace->mNumber));
            gCoasterEditInstance->mFlags &= 0xffffdfffu;
            tag_sTrackBrace* drag = gCoasterDragBrace;
            ExchangeTrackBraces(drag, PylonOf(drag)->mBraces[IndexOf(drag) - 1]);
            LockPointedAtRide(1);
            ToggleSwapDownPylonState();
            return;
        }
        gCoasterInputFlags &= ~0x10u;
    }
    LockPointedAtRide(0);
}

// 0x10053a04: the second brace of the preview range (flag set: only note a change; clear: unmark the old range first).
void SetTrackPreview2ndBrace(tag_sTrackBrace* brace, int32_t flag) {
    bool changed = false;
    if (flag == 0) {
        tag_sTrackBrace* old = gPylonMoveMark2;
        if (old != nullptr) {
            tag_sTrackBrace* walk = old;
            tag_sTrackBrace* end = gPylonMoveMark1;
            if (end->mNumber > old->mNumber) {
                walk = old;
            } else {
                walk = end;
                end = old;
            }
            for (; walk != end; walk = walk->mPrev) walk->mFlags &= 0xfeffbfffu;
            changed = true;
        }
    } else if (gPylonMoveMark2 != brace) {
        changed = true;
    }
    gPylonMoveMark2 = brace;
    if (brace != nullptr) {
        tag_sTrackBrace* start = gPylonMoveMark1;
        tag_sTrackBrace* end = brace;
        if (start->mNumber > brace->mNumber) {
            tag_sTrackBrace* swap = brace;
            end = start;
            start = swap;
        }
        for (; start != end; start = start->mPrev) start->mFlags |= 0x1000000u;
        changed = true;
    }
    if (changed) FlagGenerateTrack(gCoasterEditInstance, 0);
}

// 0x10053b58: leaves the pylon edit state the input flags name, then clears the preview words.
void DisableCurrentPylonEditState() {
    if ((gCoasterWord16E0 & 1) != 0 && gCoasterEditInstance != nullptr) {
        gCoasterEditInstance->mFlags &= ~4u;
        FlagGenerateTrack(gCoasterEditInstance, 0);
        uint32_t block = gCoasterEditInstance->mBlock3C;
        if ((block & 1) == 0 || (block & 2) != 0) FlashEndPylons(gCoasterEditInstance, 1);
    }
    const uint32_t flags = gCoasterInputFlags;
    switch (flags & 0x3f) {
        case 1:
            ToggleLoftPylonState();
            break;
        case 2:
            ToggleRotatePylonState();
            break;
        case 4: {
            tag_sCoasterInstance* instance = gCoasterEditInstance;
            if (instance == nullptr || instance->mF40 == 0) break;
            tag_sTrackBrace* drag = gCoasterDragBrace;
            if (drag == nullptr) break;
            if ((drag->mData->mFlags & 2) != 0) break;
            if ((drag->mFlags & 0x40) != 0)
                ToggleLoopTurnPylonState();
            else
                ToggleBankInclinePylonState();
            break;
        }
        case 8:
            ToggleSwapUpPylonState();
            break;
        case 0x10:
            ToggleSwapDownPylonState();
            break;
        case 0x20:
            if ((flags & 0x80) != 0) {
                LockPointedAtRide(0);
                gCoasterWord1788 = 0;
                bool marked = false;
                tag_sTrackBrace* second = gPylonMoveMark2;
                if (second != nullptr) {
                    tag_sTrackBrace* walk = second;
                    tag_sTrackBrace* end = gPylonMoveMark1;
                    if (end->mNumber > second->mNumber) {
                        walk = second;
                    } else {
                        walk = end;
                        end = second;
                    }
                    for (; walk != end; walk = walk->mPrev) walk->mFlags &= 0xfeffbfffu;
                    marked = true;
                }
                gPylonMoveMark1 = nullptr;
                gPylonMoveMark2 = nullptr;
                gExtrudeTextureKind = 0;
                if (marked) FlagGenerateTrack(gCoasterEditInstance, 0);
            }
            gCoasterInputFlags &= 0xffffff5fu;
            break;
        default:
            break;
    }
    gCoasterWord1790 = 0;
    gCoasterWord16DC = 0;
    gCoasterInputFlags &= ~0x40u;
    gCoasterWord16E0 = 0;
}
