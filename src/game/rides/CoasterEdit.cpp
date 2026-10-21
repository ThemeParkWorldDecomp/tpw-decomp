// CoasterEdit.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "analyser.h"
#include "Debug.h"
#include "FS_Online.h"
#include "helpers/CoasterEditHelpers.h"
#include "HFieldTextures.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "Mouse.h"
#include "online.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

// Unit v5967: AbortMovePylon 0x10057df4, SingleHFieldTextureChange 0x1005b840, PlacePylon (4 arguments) 0x10052a30,
// IsCoasterUseable 0x10052954, IsTrackValid 0x100528d8, IsTrackClosed (void*) 0x10052850, DecCoasterConst2 0x10052628.

// 0x10052a30: the answer is whatever the 7-argument PlacePylon left in r3.
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, uint32_t* meshSlot) {
    tag_sPylon* pylon;
    tag_sTrackBrace* brace;
    int32_t result = PlacePylon(x, z, flag, &pylon, &brace, 0xff, 0);
    if (meshSlot != nullptr) {
        if (pylon != nullptr)
            *meshSlot = brace->mWord70;
        else
            *meshSlot = 0;
    }
    return result;
}

// TPConfig.cpp (main app): the coaster editor's data change and the loft drag toggle. Unit v5054.
//   HandleCoasterDataChange 0x10052318, ToggleLoftPylonState 0x10052ac4

float gCoasterVerticalFloor;   // 0x1029ce80

namespace {

// The runtime's double to unsigned conversion (0x10237cd8) for a value in range.
uint32_t FloatToUInt(float value) { return static_cast<uint32_t>(static_cast<int64_t>(value)); }

}  // namespace

// 0x10052ac4: the loft drag starts (the flag bit 0 is set) or ends. Always answers 1.
int32_t ToggleLoftPylonState() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance == nullptr || instance->mF40 == 0) return 1;
    uint32_t flags = gCoasterInputFlags;
    tag_sTrackBrace* brace = gCoasterDragBrace;
    const uint32_t low = flags & 1;
    if (low == 0) {
        if (brace == nullptr || (ControlOf(brace->mData)->mFlags & 0x20) == 0) return 1;
    }
    if ((brace->mData->mFlags & 2) != 0) return 1;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 1;
    } else if (low != 0) {
        gActionRecorder.RecordAction(0x39, brace->mFE8w, static_cast<uint16_t>(brace->mNumber));
        brace = gCoasterDragBrace;
        if ((brace->mFlags & 0x40) != 0) {
            tag_sTrackBrace* other = (brace->mFlags & 0x80) != 0 ? brace->mPrev : brace->mNext;
            if (other != nullptr && other->mData != nullptr)
                gActionRecorder.RecordAction(0x39, other->mFE8w, static_cast<uint16_t>(other->mNumber));
        }
        gCoasterEditInstance->mFlags &= 0xffffdfffu;
        gCoasterInputFlags &= 0xfffffffeu;
    }
    if ((gCoasterInputFlags & 1) != 0) {
        gCoasterDragLastDY = 0.0f;
        std::memcpy(&gCoasterDragAnchor[0], &gPointerScreenXY[0], sizeof(float));
        std::memcpy(&gCoasterDragAnchor[1], &gPointerScreenXY[1], sizeof(float));
        brace = gCoasterDragBrace;
        if (brace == nullptr) {
            gCoasterInputFlags &= 0xfffffffeu;
        } else {
            gCoasterCostBefore = CalculateBraceCost(gCoasterEditInstance, 0, brace, nullptr, nullptr, nullptr, nullptr);
            gCoasterVerticalTarget1 = &brace->mFE8w;
            gCoasterVerticalTarget2 = nullptr;
            gCoasterHorizontalTarget1 = nullptr;
            gCoasterHorizontalTarget2 = nullptr;
            gCoasterVerticalBase1 = *gCoasterVerticalTarget1;
            sPylonControl* control = ControlOf(brace->mData);
            float interval = GetIntervalForHeight(control->mF68, control);
            gCoasterVerticalMin1 = interval < gCoasterVerticalFloor ? gCoasterVerticalFloor : interval;
            tag_sTrackBrace* dragged = gCoasterDragBrace;
            const sTrackBraceData* pylon = dragged->mData;
            // The Mac reads the word at 0x30 + count * 4: the last brace of the pylon (the count word itself, 0, when it has none).
            const sCoasterPylon* node = reinterpret_cast<const sCoasterPylon*>(pylon);
            tag_sTrackBrace* second = node->mCount == 0 ? nullptr : node->mBraces[node->mCount - 1];
            float height = CalculateBraceHeight(second);
            sPylonControl* control2 = ControlOf(pylon);
            float delta = GetIntervalForHeightDelta(control2->mF6C - height, control2);
            gCoasterVerticalMax1 = delta + dragged->mFE8w;
            gCoasterVerticalBase2 = 0.0f;
            gCoasterDragScaleY = 0.005f;
            gCoasterInputFlags |= 0x400;
            LockPointedAtRide(1);
            return 1;
        }
    } else {
        if (gActionRecorder.mRecording == 0) {
            float cost = CalculateBraceCost(gCoasterEditInstance, 0, gCoasterDragBrace, nullptr, nullptr, nullptr, nullptr);
            int32_t paid = RoundUp(cost);
            CBankAccount* bank = gGameWorld->GetBankAccount();
            bank->MakeDeposit(FloatToUInt(gCoasterCostBefore));
            bank->GetBalance();
            bank->MakeWithdrawal(static_cast<uint32_t>(paid));
            gCoasterCostBefore = 0.0f;
        }
        gCoasterInputFlags &= 0xffffe1ffu;
    }
    LockPointedAtRide(0);
    return 1;
}

// The editor's drag starts for a pylon: loop turn 0x10052e5c, rotate 0x100530a8, bank and incline 0x100532f4.

namespace {

sPylonControl* PylonControlOf(const sTrackBraceData* pylon) { return reinterpret_cast<const sCoasterPylon*>(pylon)->mControl; }

float FloatOfBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void CopyPointerPosition() {
    std::memcpy(&gCoasterDragAnchor[0], &gPointerScreenXY[0], sizeof(float));
    std::memcpy(&gCoasterDragAnchor[1], &gPointerScreenXY[1], sizeof(float));
}

}  // namespace

// 0x10052e5c: the loop turn drag (flag 4) starts or ends. Always answers 1 (the Mac's r3); the header declares it void.
void ToggleLoopTurnPylonState() {
    const uint32_t flags = gCoasterInputFlags;
    const uint32_t low = flags & 4;
    if (low == 0) {
        if (gCoasterDragBrace == nullptr || (PylonControlOf(gCoasterDragBrace->mData)->mFlags & 0x200) == 0) return;
    }
    tag_sTrackBrace* brace = gCoasterDragBrace;
    if ((brace->mData->mFlags & 2) != 0) return;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 4;
    } else if (low != 0) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x3b), brace->mFF8, 0.0f, static_cast<uint16_t>(brace->mNumber));
        gCoasterEditInstance->mFlags &= 0xffffdfffu;
        gCoasterInputFlags &= 0xfffffffbu;
    }
    if ((gCoasterInputFlags & 4) != 0) {
        gCoasterDragLastDY = 0.0f;
        CopyPointerPosition();
        brace = gCoasterDragBrace;
        if (brace != nullptr) {
            gCoasterVerticalTarget1 = nullptr;
            gCoasterVerticalTarget2 = nullptr;
            if ((brace->mFlags & 0x80) != 0 && brace->mFF8 > 0.5f) brace->mFF8 = brace->mFF8 - 1.0f;
            brace = gCoasterDragBrace;
            gCoasterHorizontalTarget1 = &brace->mFF8;
            gCoasterHorizontalTarget2 = nullptr;
            gCoasterHorizontalBase1 = *gCoasterHorizontalTarget1;
            gCoasterHorizontalBase2 = 0.0f;
            if ((brace->mFlags & 0x80) != 0) {
                gCoasterHorizontalMin1 = FloatOfBits(0xbeb33333u);
                gCoasterHorizontalMax1 = FloatOfBits(0x3eb33333u);
            } else {
                gCoasterHorizontalMin1 = FloatOfBits(0x3e19999au);
                gCoasterHorizontalMax1 = FloatOfBits(0x3f59999au);
            }
            gCoasterDragScaleX = FloatOfBits(0x3c23d70au);
            gCoasterInputFlags |= 0x200;
            LockPointedAtRide(1);
            return;
        }
        gCoasterInputFlags &= 0xfffffffbu;
    } else {
        gCoasterInputFlags &= 0xffffe1ffu;
    }
    LockPointedAtRide(0);
}

// 0x100530a8: the rotate drag (flag 2) starts or ends. Always answers 1.
void ToggleRotatePylonState() {
    if (gCoasterEditInstance == nullptr || gCoasterEditInstance->mF40 == 0) return;
    const uint32_t flags = gCoasterInputFlags;
    const uint32_t low = flags & 2;
    if (low == 0) {
        if (gCoasterDragBrace == nullptr || (PylonControlOf(gCoasterDragBrace->mData)->mFlags & 0x40) == 0) return;
    }
    tag_sTrackBrace* brace = gCoasterDragBrace;
    if ((brace->mData->mFlags & 2) != 0) return;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 2;
    } else if (low != 0) {
        gActionRecorder.RecordAction(0x3a, brace->mAngle, static_cast<uint16_t>(brace->mNumber));
        gCoasterEditInstance->mFlags &= 0xffffdfffu;
        gCoasterInputFlags &= 0xfffffffdu;
    }
    if ((gCoasterInputFlags & 2) != 0) {
        gCoasterDragLastDX = 0.0f;
        CopyPointerPosition();
        brace = gCoasterDragBrace;
        if (brace != nullptr) {
            gCoasterVerticalTarget1 = nullptr;
            gCoasterVerticalTarget2 = nullptr;
            gCoasterHorizontalTarget1 = &brace->mAngle;
            gCoasterHorizontalTarget2 = nullptr;
            gCoasterHorizontalBase1 = *gCoasterHorizontalTarget1;
            gCoasterHorizontalMin1 = brace->mFD4 - PylonControlOf(brace->mData)->mAngles[8];
            gCoasterHorizontalMax1 = brace->mFD4 + PylonControlOf(brace->mData)->mAngles[8];
            gCoasterDragScaleX = FloatOfBits(0x3ca3d70au);
            gCoasterInputFlags |= 0x200;
            brace->mFlags |= 1;
            LockPointedAtRide(1);
            return;
        }
        gCoasterInputFlags &= 0xfffffffdu;
    } else {
        gCoasterInputFlags &= 0xffffe1ffu;
    }
    LockPointedAtRide(0);
}

// 0x100532f4: the bank (flag 4, 0x100 control bit) and incline (0x400, 0x80 control bit) drags start or end. Always answers 1.
void ToggleBankInclinePylonState() {
    const uint32_t flags = gCoasterInputFlags;
    const uint32_t low = flags & 4;
    if (low == 0) {
        if (gCoasterDragBrace == nullptr || (PylonControlOf(gCoasterDragBrace->mData)->mFlags & 0x180) == 0) return;
    }
    tag_sTrackBrace* brace = gCoasterDragBrace;
    if ((brace->mData->mFlags & 2) != 0) return;
    if ((flags & 0x3f) == 0) {
        gCoasterInputFlags |= 0x2004;
    } else if (low != 0) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x3b), brace->mFD8, brace->mFDC, static_cast<uint16_t>(brace->mNumber));
        gCoasterEditInstance->mFlags &= 0xffffdfffu;
        gCoasterInputFlags &= 0xffffdffbu;
    }
    if ((gCoasterInputFlags & 4) != 0) {
        gCoasterDragLastDX = 0.0f;
        gCoasterDragLastDY = 0.0f;
        CopyPointerPosition();
        brace = gCoasterDragBrace;
        if (brace != nullptr) {
            if ((PylonControlOf(brace->mData)->mFlags & 0x100) != 0) {
                gCoasterHorizontalTarget1 = &brace->mFD8;
                gCoasterHorizontalTarget2 = nullptr;
                gCoasterHorizontalBase1 = *gCoasterHorizontalTarget1;
                gCoasterHorizontalMin1 = FloatOfBits(0xc0490e38u);
                gCoasterHorizontalMax1 = FloatOfBits(0x40490e38u);
                gCoasterDragScaleX = FloatOfBits(0x3ca3d70au);
                gCoasterInputFlags |= 0x200;
                brace->mFlags |= 4;
            }
            brace = gCoasterDragBrace;
            if ((PylonControlOf(brace->mData)->mFlags & 0x80) != 0) {
                gCoasterVerticalTarget1 = &brace->mFDC;
                gCoasterVerticalTarget2 = nullptr;
                gCoasterVerticalBase1 = *gCoasterVerticalTarget1;
                gCoasterVerticalMin1 = FloatOfBits(0xc0490e38u);
                gCoasterVerticalMax1 = FloatOfBits(0x40490e38u);
                gCoasterDragScaleY = FloatOfBits(0x3ca3d70au);
                gCoasterInputFlags |= 0x400;
                brace->mFlags |= 2;
            }
            LockPointedAtRide(1);
            return;
        }
        gCoasterInputFlags &= 0xfffffffbu;
    } else {
        gCoasterInputFlags &= 0xffffe1ffu;
    }
    LockPointedAtRide(0);
}

// Unit v5084: the pylon edit states of the track editor (TPConfig.cpp; names invented):
//   ExchangeTrackBraces 0x10053594, ToggleSwapUpPylonState 0x10053720, ToggleSwapDownPylonState 0x10053898,
//   SetTrackPreview2ndBrace 0x10053a04, DisableCurrentPylonEditState 0x10053b58

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

// 0x10053d88
void ClearCoasterButtons() {
    DisableCurrentPylonEditState();
    gCoasterEditEnabled = 0;
    gCoasterEditDown = 0;
}

// 0x10053de8: leaves the pylon edit state, clears the two button masks, then refreshes the UI.
void SetCoasterEditState(uint32_t state) {
    DisableCurrentPylonEditState();
    gCoasterEditEnabled = 0;
    gCoasterEditDown = 0;
    SetCoasterButtonState(state, 0);
    GetCoasterEditAvailableButtons(nullptr);
    gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    gUIInterface.ChangeCoasterButtonsDown(gCoasterEditDown);
}

// 0x10053eac: clears the join flags of the two braces at +0x60 and +0x64, makes +0x60 the head (+0x58) again, regenerates.
void RejoinTrack(tag_sCoasterInstance* instance) {
    // (the original's mPreviewBrace flags are tested after the first store; mBrace64 may alias it)
    // +0x60 is mPreviewBrace, +0x64 mBrace64, +0x58 mBraces
    instance->mPreviewBrace->mFlags &= 0xfffaffffu;
    if ((instance->mPreviewBrace->mFlags & 0x400) != 0)
        instance->mBrace64->mFlags &= 0xfffdffffu;
    else
        instance->mBrace64->mFlags &= 0xfff5ffffu;
    instance->mPreviewBrace = instance->mBraces;
    instance->mBlock3C &= ~2u;
    SetCoasterState(instance, 2);
    FlagGenerateTrack(instance, 1);
}

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

// 0x10054554: always answers 1; marks the loop placing when the editor's coaster has the type bit.
int32_t SetPlaceLoop() {
    uint32_t flags = gCoasterInputFlags;
    if ((flags & 0x40) != 0) return 1;
    if (gCoasterEditInstance == nullptr) return 1;
    if ((gCoasterEditInstance->mType[0] & 2) == 0) return 1;
    gCoasterInputFlags = flags | 0x40;
    gCoasterWord1790 = 2;
    return 1;
}

// 0x100545d4
int32_t OutputCoasterValues() {
    gCoasterWord7310 = 1;
    FlagGenerateTrack(gCoasterEditInstance, 0);
    return 1;
}

// Unit v5893: RemoveTrackBraceFromPylon 0x10054640, DeletePylon 0x10054814, MoveBrace 0x10054978 (TPConfig.cpp; names invented).
// The pylon a brace points at is an sPylonRecord on the host (native pointers; the Mac offsets are in TPCoaster.h).

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

// The coasters' per-frame ticks and the track editor's pointer input (main app TPCoaster.cpp and the TPConfig.cpp range, unit w137):
// CoasterTickPreRender 0x10043028 (428 bytes), CoasterTickPostRender 0x10043200 (176) and CoasterUserInputTick 0x100554b4 (2,348).
// The globals' places are in include/TPCoaster.h (names invented); every callee outside the unit is untranslated.

extern tag_sCoasterInstance* gCoasterEditInstance;

uint32_t gCoasterInputFlags;
tag_sTrackBrace* gCoasterDragBrace;
float gCoasterDragAnchor[2];
float gCoasterDragScaleX;
float gCoasterDragScaleY;
float gCoasterDragLastDX;
float gCoasterDragLastDY;
float gCoasterSavedVertical1[3];
float gCoasterSavedVertical2[3];
float gCoasterSavedHorizontal1[3];
float gCoasterSavedHorizontal2[3];
float* gCoasterVerticalTarget1;
float* gCoasterVerticalTarget2;
float* gCoasterHorizontalTarget1;
float* gCoasterHorizontalTarget2;
float gCoasterVerticalBase1;
float gCoasterVerticalMin1;
float gCoasterVerticalMax1;
float gCoasterVerticalBase2;
float gCoasterVerticalMin2;
float gCoasterVerticalMax2;
float gCoasterHorizontalBase1;
float gCoasterHorizontalMin1;
float gCoasterHorizontalMax1;
float gCoasterHorizontalBase2;
float gCoasterHorizontalMin2;
float gCoasterHorizontalMax2;
float gCoasterCostBefore;
int32_t gCoasterGridCellX;
int32_t gCoasterGridCellZ;
tag_sRideAnimSet* gCoasterPreviousPointedRide;
uint32_t gCoasterWord16E0;

namespace {

constexpr float kHalfPi = 1.5707964f;   // 0x3fc90fdb (TOC 0x102648e4)

// The sine table at an angle in radians: the product with the scale in single precision, truncated (saturating), and-ed with the mask.
float TableAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

// The sine and the cosine of an angle as the input tick reads them (the cosine at pi/2 plus the angle).
struct SinCos {
    float sine;
    float cosine;
};
SinCos SinCosAt(float angle) {
    const float scale = gTPGlobals.mSinTableScale;
    const float sine = TableAt(angle * scale);
    const float cosine = TableAt((kHalfPi + angle) * scale);
    return {sine, cosine};
}

// Turns the point about the y axis (x and z change): x' = x cos + z sin, z' = -x sin + z cos, each fused as the listing has it.
void TurnXZ(float* p, const SinCos& t) {
    const float x = p[0];
    p[0] = ppc_fmadds(x, t.cosine, p[2] * t.sine);
    p[2] = ppc_fmadds(ppc_fneg(x), t.sine, p[2] * t.cosine);
}

// About the x axis (y and z change): y' = y cos - z sin, z' = y sin + z cos.
void TurnYZ(float* p, const SinCos& t) {
    const float y = p[1];
    p[1] = ppc_fmsubs(y, t.cosine, p[2] * t.sine);
    p[2] = ppc_fmadds(y, t.sine, p[2] * t.cosine);
}

// The vertical drag of a target that is turned: the saved point copied in, then turned about y by the angle, about x by the delta
// and about y by the angle back.
void TurnSavedPoint(float* target, const float* saved, float angle, float delta) {
    std::memcpy(target, saved, 3 * sizeof(float));
    TurnXZ(target, SinCosAt(ppc_fneg(angle)));
    TurnYZ(target, SinCosAt(delta));
    TurnXZ(target, SinCosAt(angle));
}

// A clamp of a target's first float between the minimum and the maximum (the compares are fcmpo: a NaN takes the first branch).
// Stores the limit and returns 1 when the value went to the minimum, 2 when to the maximum, 0 when it was inside.
int Clamp(float* target, float minimum, float maximum) {
    const float value = *target;
    if (!(value >= minimum)) {
        *target = minimum;
        return 1;
    }
    if (!(value <= maximum)) {
        *target = maximum;
        return 2;
    }
    return 0;
}

}  // namespace

// 0x100554b4. Moves the brace the player drags in the track editor: the pointer's offset from the drag's anchor, scaled, is turned by
// the camera yaw (flag 0x2000), and drives the vertical targets (flags 0x400 move, 0x1000 turn saved points) and the horizontal ones
// (0x200, 0x800). The first of a pair also moves the anchor back when it clamps (without 0x2000). Then the multi-brace preview, the
// data change and the cost of the move (a refund adds -0xeeeeeee).
void CoasterUserInputTick() {
    bool changed = false;
    if (gCoasterEditInstance == nullptr || gCoasterEditInstance->mF40 == 0) return;
    float yaw;
    if (gCoasterDragBrace == nullptr) {
        yaw = gCameraF5BF4;
    } else {
        yaw = gCameraF5BF4 - gCoasterDragBrace->mAngle;
    }
    float dx = 0.0f;
    float dy = 0.0f;
    uint32_t flags = gCoasterInputFlags;
    if ((flags & 0x3f) != 0 && (flags & 0x1e00) != 0) {
        dx = gCoasterDragScaleX * (gCoasterDragAnchor[0] - gPointerScreenXY[0]);
        dy = gCoasterDragScaleY * (gCoasterDragAnchor[1] - gPointerScreenXY[1]);
        if ((flags & 0x2000) != 0) {
            const SinCos t = SinCosAt(yaw);
            const float newDX = ppc_fmsubs(dx, t.cosine, dy * t.sine);
            const float newDY = ppc_fmadds(dx, t.sine, dy * t.cosine);
            dx = newDX;
            dy = newDY;
        }
        // The vertical drags: the delta of y, or (with a horizontal drag) of x, changed since the last tick.
        if ((flags & 0x1400) != 0 &&
            (((flags & 0xa00) != 0 && dx != gCoasterDragLastDX) || dy != gCoasterDragLastDY)) {
            if ((flags & 0x400) != 0) {
                float* target = gCoasterVerticalTarget1;
                *target = gCoasterVerticalBase1 + dy;
                const int clamped = Clamp(target, gCoasterVerticalMin1, gCoasterVerticalMax1);
                if (clamped != 0 && (gCoasterInputFlags & 0x2000) == 0) {
                    gCoasterDragAnchor[1] = gPointerScreenXY[1] + (*target - gCoasterVerticalBase1) / gCoasterDragScaleY;
                }
                float* second = gCoasterVerticalTarget2;
                if (second != nullptr) {
                    *second = gCoasterVerticalBase2 + dy;
                    Clamp(second, gCoasterVerticalMin2, gCoasterVerticalMax2);
                }
            } else if ((flags & 0x1000) != 0) {
                float* target = gCoasterVerticalTarget1;
                std::memcpy(target, gCoasterSavedVertical1, 3 * sizeof(float));
                const float angle = GetVectorRotationAroundYAxisFromZ(*reinterpret_cast<s3DPoint*>(target));
                TurnXZ(target, SinCosAt(ppc_fneg(angle)));
                TurnYZ(target, SinCosAt(dy));
                TurnXZ(target, SinCosAt(angle));
                float* second = gCoasterVerticalTarget2;
                if (second != nullptr) TurnSavedPoint(second, gCoasterSavedVertical2, angle, dy);
            }
            changed = true;
            gCoasterDragLastDY = dy;
        }
        flags = gCoasterInputFlags;
        // The horizontal drags: the delta of x changed, or (with a vertical drag) the delta of y.
        if ((flags & 0xa00) != 0 &&
            (dx != gCoasterDragLastDX || ((flags & 0x1400) != 0 && dy != gCoasterDragLastDY))) {
            if ((flags & 0x200) != 0) {
                float* target = gCoasterHorizontalTarget1;
                *target = gCoasterHorizontalBase1 + dx;
                const int clamped = Clamp(target, gCoasterHorizontalMin1, gCoasterHorizontalMax1);
                if (clamped != 0 && (gCoasterInputFlags & 0x2000) == 0) {
                    gCoasterDragAnchor[0] = gPointerScreenXY[0] + (*target - gCoasterHorizontalBase1) / gCoasterDragScaleX;
                }
                float* second = gCoasterHorizontalTarget2;
                if (second != nullptr) {
                    *second = gCoasterHorizontalBase2 + dx;
                    Clamp(second, gCoasterHorizontalMin2, gCoasterHorizontalMax2);
                }
            } else if ((flags & 0x800) != 0) {
                float* target = gCoasterHorizontalTarget1;
                std::memcpy(target, gCoasterSavedHorizontal1, 3 * sizeof(float));
                TurnXZ(target, SinCosAt(dx));
                float* second = gCoasterHorizontalTarget2;
                if (second != nullptr) {
                    std::memcpy(second, gCoasterSavedHorizontal2, 3 * sizeof(float));
                    TurnXZ(second, SinCosAt(dx));
                }
            }
            changed = true;
            gCoasterDragLastDX = dx;
        }
    }
    flags = gCoasterInputFlags;
    if ((flags & 0x13f) != 0) {
        gCoasterGridCellX = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f));
        gCoasterGridCellZ = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f));
        if ((flags & 0x180) != 0) {
            HandleMultiBracePreviewChange(gCoasterEditInstance, gCoasterPreviousPointedRide, gPointedRide,
                                          gCoasterPreviousPointedRide != gPointedRide, static_cast<uint32_t>(gCoasterGridCellX),
                                          static_cast<uint32_t>(gCoasterGridCellZ));
        }
        gCoasterPreviousPointedRide = gPointedRide;
    }
    if (changed && gCoasterDragBrace != nullptr) {
        HandleCoasterDataChange(gCoasterInputFlags, gCoasterDragBrace, gCoasterEditInstance, 0, 0);
    }
    if ((gCoasterInputFlags & 0x3f) != 0 && gCoasterDragBrace != nullptr && (gCoasterDragBrace->mData->mFlags & 2) == 0 &&
        gActionRecorder.mRecording == 0) {
        const int32_t before = ppc_fctiwz(static_cast<double>(gCoasterCostBefore));
        const float cost = CalculateBraceCost(gCoasterEditInstance, 0, gCoasterDragBrace, nullptr, nullptr, nullptr, nullptr);
        const int32_t difference = static_cast<int32_t>(static_cast<uint32_t>(RoundUp(cost)) - static_cast<uint32_t>(before));
        if (difference < 0) {
            // A refund: the marker -0xeeeeeee is added (addis -0xeef, addi 0x1112).
            LLCC_SystemAddToSessionCost(static_cast<int32_t>(static_cast<uint32_t>(difference) - 0xeeeeeeeu));
        } else {
            LLCC_SystemAddToSessionCost(difference);
        }
    }
    if ((gCoasterWord16E0 & 1) != 0) FlagGenerateTrack(gCoasterEditInstance, 0);
}

// Pylon editing setters of the editor's coaster (TPConfig.cpp range 0x10055e0c..0x10056500).

uint32_t gDrawUpSplineFlag;   // 0x102b74a4

static tag_sTrackBrace* EditBrace(uint32_t id) {
    return GetTrackBrace(reinterpret_cast<sCoasterTrackParams*>(&gCoasterEditInstance->mBlock3C), id);
}

// 0x10055e0c
void SetLoftPylon(float value, uint32_t id, int32_t a, int32_t b) {
    tag_sTrackBrace* brace = EditBrace(id);
    brace->mFE8w = value;
    HandleCoasterDataChange(1, brace, gCoasterEditInstance, a, b);
}

// 0x10055eac
void LayoutSetLoftPylon(float value, uint32_t id) {
    tag_sTrackBrace* brace = EditBrace(id);
    brace->mFE8w = value;
    HandleCoasterDataChange(1, brace, gCoasterEditInstance, 0, 0);
}

// 0x10055f38
void SetRotatePylon(float value, uint32_t id, int32_t setFlag, int32_t b) {
    tag_sTrackBrace* brace = EditBrace(id);
    brace->mAngle = value;
    if (setFlag != 0) brace->mFlags |= 1;
    HandleCoasterDataChange(2, brace, gCoasterEditInstance, 0, b);
}

// 0x10055ff0
void LayoutSetRotatePylon(float value, uint32_t id) {
    tag_sTrackBrace* brace = EditBrace(id);
    brace->mAngle = value;
    brace->mFlags |= 1;
    HandleCoasterDataChange(2, brace, gCoasterEditInstance, 0, 0);
}

// 0x1005608c
void SetWobblePylon(float x, float z, uint32_t id, int32_t setFlags, int32_t b) {
    tag_sTrackBrace* brace = EditBrace(id);
    if ((brace->mFlags & 0x40) != 0) {
        brace->mFD8 = 0.0f;
        brace->mFDC = 0.0f;
        brace->mFF8 = x;
    } else {
        brace->mFD8 = x;
        brace->mFDC = z;
        brace->mFF8 = 0.0f;
        if (setFlags != 0) brace->mFlags |= 6;
    }
    HandleCoasterDataChange(4, brace, gCoasterEditInstance, 0, b);
}

// 0x10056180
void LayoutSetWobblePylon(float x, float z, uint32_t id) {
    tag_sTrackBrace* brace = EditBrace(id);
    if ((brace->mFlags & 0x40) != 0) {
        brace->mFD8 = 0.0f;
        brace->mFDC = 0.0f;
        brace->mFF8 = x;
    } else {
        brace->mFD8 = x;
        brace->mFDC = z;
        brace->mFF8 = 0.0f;
        brace->mFlags |= 6;
    }
    HandleCoasterDataChange(4, brace, gCoasterEditInstance, 0, 0);
}

// 0x10056258
int32_t ToggleDrawUpSpline() {
    gDrawUpSplineFlag = (gDrawUpSplineFlag == 0) ? 1 : 0;
    if (gCoasterEditInstance != nullptr) FlagGenerateTrack(gCoasterEditInstance, 0);
    return 1;
}

// 0x100562d4
void SetPylonType(uint32_t type) {
    if (type == 2) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x38));
        gCoasterEditInstance->mFlags &= ~0x2000u;
        tag_sCoasterInstance* inst = gCoasterEditInstance;
        bool found = false;
        if (inst != nullptr && (inst->mType[0] & 2)) {
            if ((inst->mBlock3C & 1) == 0) {
                if (inst->mBraces->mFlags & 0x80) found = true;
            } else if (inst->mBlock3C & 2) {
                if (inst->mPreviewBrace->mFlags & 0x80) found = true;
            }
        }
        if (found) {
            if (inst != nullptr && (inst->mType[0] & 2)) {
                gCoasterInputFlags |= 0x40;
                gCoasterWord1790 = 1;
            }
        } else if ((gCoasterInputFlags & 0x40) == 0) {
            if (inst != nullptr && (inst->mType[0] & 2)) {
                gCoasterInputFlags |= 0x40;
                gCoasterWord1790 = 2;
            }
        }
    } else {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x37));
        gCoasterEditInstance->mFlags &= ~0x2000u;
        if (gCoasterWord16DC == 2 && gCoasterWord1790 == 2) {
            if (gCoasterEditInstance != nullptr && (gCoasterEditInstance->mType[0] & 2)) {
                gCoasterInputFlags &= ~0x40u;
                gCoasterWord1790 = 0;
            }
        }
        // the original clears the word again whatever the test said
        gCoasterWord1790 = 0;
    }
    gCoasterWord16DC = type;
    SetPotentialPylonArea();
}

// 0x100564ac
void CorrectBraceNumbering(tag_sTrackBrace* brace) {
    if (brace == nullptr) return;
    int32_t n = (brace->mNext != nullptr) ? static_cast<int32_t>(brace->mNext->mNumber) + 1 : 0;
    for (; brace != nullptr; brace = brace->mPrev) {
        brace->mNumber = static_cast<uint32_t>(n);
        brace->mF6C = static_cast<float>(static_cast<uint32_t>(n));   // the magic-number conversion has no sign flip: unsigned
        n++;
    }
}

// 0x10056548
void UIHandleMidLoop() {
    UIInterface* ui = &gUIInterface;
    GetCoasterEditAvailableButtons(nullptr);
    ui->ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    ui->ChangeCoasterButtonsDown(gCoasterEditDown);
}

// 0x100565cc
void UIHandleEndLoop() {
    UIInterface* ui = &gUIInterface;
    GetCoasterEditAvailableButtons(nullptr);
    SetCoasterButtonState(1, 2);
    ui->ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    ui->ChangeCoasterButtonsDown(gCoasterEditDown);
}

// DeleteTrackBrace 0x10056660 and DeleteLastTrackBrace 0x10056a80 (unit v5337).

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

sPylonControl* ControlOf_2(const tag_sPylon* pylon) {
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
            int32_t index = ControlOf_2(cur->mData)->mF2C;
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
    DestroyTrackBrace(brace, ControlOf_2(pylon));
    if (pylon->mF30 == 0) {
        DeletePylon(instance, pylon, 0);
    } else {
        uint32_t ownerWord;
        std::memcpy(&ownerWord, reinterpret_cast<const uint8_t*>(pylon->mOwner) + 0x70, sizeof(ownerWord));
        Assign3DObjectToAIWorld(instance, pylon->mCellX, pylon->mCellZ, ownerWord, 0);
        UpdatePylonStackStructures(ControlOf_2(pylon), pylon);
        UpdatePylonStackMeshes(ControlOf_2(pylon), pylon, 0, 0);
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

namespace {

// The inlined test the three functions repeat: the editor's coaster has a type with flag 2 and the brace it edits
// (the list head, or the preview brace when the block's flag 2 is set) carries flag 0x80.
bool EditBraceMarked(tag_sCoasterInstance* inst) {
    if (inst == nullptr) return false;
    if ((inst->mType[0] & 2) == 0) return false;
    uint32_t flags = inst->mBlock3C;
    if ((flags & 1) == 0) return (inst->mBraces->mFlags & 0x80) != 0;
    return (flags & 2) != 0 && (inst->mPreviewBrace->mFlags & 0x80) != 0;
}

}  // namespace

// 0x10056d7c
void HandleCoasterButtonDown(uint32_t buttons) {
    switch (buttons) {
        case 1:
        case 2:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 1;
            FlashEndPylons(gCoasterEditInstance, 2);
            gCoasterEditInstance->mFlags |= 4;
            if (EditBraceMarked(gCoasterEditInstance)) {
                gCoasterInputFlags |= 0x40;
                gCoasterWord1790 = 1;
            }
            SetPylonType(buttons);
            break;
        case 4:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 2;
            break;
        case 8:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 4;
            break;
        case 0x10:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 8;
            break;
        case 0x20:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0x10;
            break;
        case 0x40:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0x80;
            break;
        case 0x80:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0x20;
            break;
        case 0x100:
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0x40;
            break;
        default:
            break;
    }
    gCoasterEditDown |= buttons;
}

// 0x10057064
void SetCoasterButtonState(uint32_t state, uint32_t arg) {
    uint32_t bit = 1;
    while (arg != 0) {
        uint32_t hit = arg & bit;
        if (hit != 0) {
            if ((hit & 0x1ff) != 0) {
                DisableCurrentPylonEditState();
                gCoasterEditDown &= ~hit;
            }
            arg &= ~bit;
        }
        bit <<= 1;
    }
    bit = 1;
    while (state != 0) {
        if ((state & bit) != 0) {
            HandleCoasterButtonDown(state & bit);
            state &= ~bit;
        }
        bit <<= 1;
    }
}

// 0x10057120
void GetCoasterEditDefaultButtons(uint32_t* out) {
    tag_sCoasterInstance* inst = gCoasterEditInstance;
    uint32_t flags = inst->mBlock3C;
    if ((flags & 1) != 0 && (flags & 2) == 0) {
        *out = 0x10;
    } else {
        if (EditBraceMarked(inst)) {
            gCoasterInputFlags |= 0x40;
            gCoasterWord1790 = 1;
        }
        *out = (gCoasterWord1790 == 1) ? 2 : 1;
    }
    HandleCoasterButtonDown(*out);
}

// 0x10057278
void GetCoasterEditAvailableButtons(uint32_t* out) {
    uint32_t local;
    if (out == nullptr) out = &local;
    // The original tests the pressed buttons in four tiers (1, 4, 0x38, 0x1c0); every tier computes the same mask.
    *out = 0x1fc;
    if (!EditBraceMarked(gCoasterEditInstance)) *out |= 1;
    if ((gCoasterEditInstance->mType[0] & 0x80) != 0) {
        *out |= 2;
        if ((gCoasterEditInstance->mType[0] & 0x100) != 0) *out |= 0x200;
    }
    uint32_t flags = gCoasterEditInstance->mBlock3C;
    if ((flags & 1) != 0 && (flags & 2) == 0)
        *out &= ~3u;
    else
        *out &= ~0x40u;
    gCoasterEditEnabled = *out;
}

// Unit v5026: dragging a pylon of the track editor (TPConfig.cpp on the Mac).
//   StartMovePylon 0x10057738 (604 bytes), DoMovePylon 0x100579cc (1,024 bytes)

// 0x10057738: picks the brace up. A pylon whose record has bit 2 set cannot be moved (answers false before anything is touched).
bool StartMovePylon(tag_sTrackBrace* brace) {
    if (brace != nullptr && (brace->mData->mFlags & 2) != 0) return false;
    gCoasterWord16BC = 0;
    bool second = false;
    if (brace == nullptr) return second;

    gCoasterInputFlags |= 0x100;
    second = brace->mData->mF30 > 1;
    gPylonMoveBrace = brace;
    gPylonMoveOrigData = const_cast<tag_sPylon*>(brace->mData);
    gPylonMoveHeight = CalculateBraceHeight(brace);
    gActionRecorder.RecordAction(0x3c, 0, 0, static_cast<uint16_t>(brace->mNumber));

    gCoasterEditInstance->mFlags &= 0xffffdfffu;
    if (brace->mNext != nullptr) {
        gPylonMoveMark1 = brace->mNext;
        gExtrudeTextureKind = 3;
    } else {
        gPylonMoveMark1 = brace;
        gExtrudeTextureKind = 3;
    }
    if (brace->mPrev != nullptr)
        SetTrackPreview2ndBrace(brace->mPrev, 0);
    else
        SetTrackPreview2ndBrace(brace, 0);

    int32_t x = static_cast<int32_t>(brace->mData->mCellX);
    int32_t z = static_cast<int32_t>(brace->mData->mCellZ);
    gPylonMoveX = x;
    gPylonMoveZ = z;
    if (x < 0) x = 0;
    if (z < 0) z = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (z >= gHeightfieldHeight) z = gHeightfieldHeight - 1;
    LLCC_StartPylonMove(x, z, brace->mData->mF30 > 1);
    LockPointedAtRide(1);
    SetPotentialPylonArea();
    gCoasterCostBefore = CalculateBraceCost(gCoasterEditInstance, 0, brace, nullptr, nullptr, nullptr, nullptr);
    gPylonSecondCost = CalculateBraceCost(gCoasterEditInstance, 0, brace->mPrev, nullptr, nullptr, nullptr, nullptr);
    return second;
}

// 0x100579cc: drops the dragged brace on the cell (x, z) and pays what the move cost. Answers the brace's word 0x70 when the drop was taken.
uint32_t DoMovePylon(uint32_t x, uint32_t z) {
    gCoasterWord16BC = 0;
    bool flagged = false;
    bool moved = false;
    tag_sTrackBrace* brace;
    if (gCoasterWord16E0 != 2 || (brace = gPylonMoveBrace) == nullptr) {
        LLCC_ResetPylonMove();
        gCoasterInputFlags &= 0xfffffeffu;
        gPylonMoveBrace = nullptr;
        if (gActionRecorder.mRecording != 0) {
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0;
        }
        return 0;
    }

    if (gPylonMoveMark2 != nullptr) {
        tag_sTrackBrace* lo;
        tag_sTrackBrace* hi;
        if (gPylonMoveMark1->mNumber > gPylonMoveMark2->mNumber) {
            lo = gPylonMoveMark2;
            hi = gPylonMoveMark1;
        } else {
            lo = gPylonMoveMark1;
            hi = gPylonMoveMark2;
        }
        for (; lo != hi; lo = lo->mPrev) lo->mFlags &= 0xfeffbfffu;
        flagged = true;
    }
    gPylonMoveMark1 = nullptr;
    gPylonMoveMark2 = nullptr;
    gExtrudeTextureKind = 0;
    if (flagged) FlagGenerateTrack(gCoasterEditInstance, 0);

    tag_sPylon* pylon = GetPylonAtCell(x, z, nullptr);
    bool different = false;
    if (pylon != nullptr) {
        const tag_sPylon* data = brace->mData;
        if (pylon->mF18 != data->mF18 || pylon->mF14 != data->mF14) different = true;
    }

    if (different) {
        MoveBrace(gCoasterEditInstance, brace, gPylonMoveOrigData, static_cast<uint32_t>(gPylonMoveX), static_cast<uint32_t>(gPylonMoveZ), nullptr,
                  nullptr, 1, 0);
        LLCC_AbortPylonMove();
        if ((pylon->mFlags & 2) == 0)
            Assign3DObjectToAIWorld(gCoasterEditInstance, pylon->mCellX, pylon->mCellZ, pylon->mOwner->mF70, 0);
    } else {
        float a;
        float b;
        MoveBrace(gCoasterEditInstance, brace, pylon, x, z, &a, &b, 1, 0);
        if (gActionRecorder.mRecording == 0 &&
            (static_cast<uint32_t>(gPylonMoveX) != x || static_cast<uint32_t>(gPylonMoveZ) != z)) {
            int32_t cost;
            if ((gCoasterEditInstance->mBlock3C & 1) != 0) {
                tag_sTrackBrace* b1 = gPylonMoveBrace;
                cost = RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, b1, &a, &b, nullptr, nullptr));
                // The original tests the address of gPylonSecondCost against NULL: always true.
                cost += RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, b1->mPrev, nullptr, nullptr, &a, &b));
                CBankAccount* bank = gGameWorld->GetBankAccount();
                bank->MakeDeposit(cw_cvt_fp2unsigned(gCoasterCostBefore));
                bank->MakeDeposit(cw_cvt_fp2unsigned(gPylonSecondCost));
                bank->GetBalance();
                bank->MakeWithdrawal(static_cast<uint32_t>(cost));
                gCoasterCostBefore = 0.0f;
            } else {
                cost = RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, gPylonMoveBrace, &a, &b, nullptr, nullptr));
                CBankAccount* bank = gGameWorld->GetBankAccount();
                bank->MakeDeposit(cw_cvt_fp2unsigned(gCoasterCostBefore));
                bank->GetBalance();
                bank->MakeWithdrawal(static_cast<uint32_t>(cost));
                gCoasterCostBefore = 0.0f;
            }
        }
        moved = true;
    }

    FlagGenerateTrack(gCoasterEditInstance, 1);
    LockPointedAtRide(0);
    gPylonMoveBrace = nullptr;
    gPylonMoveOrigData = nullptr;
    gCoasterInputFlags &= 0xfffffeffu;
    if (gActionRecorder.mRecording != 0) {
        DisableCurrentPylonEditState();
        gCoasterWord16E0 = 0;
    }
    return moved ? brace->mWord70 : 0;
}

// 0x10057df4
void AbortMovePylon() {
    bool generate = false;
    gCoasterWord16BC = 0;
    if (gCoasterInputFlags & 0x100) {
        gCoasterInputFlags &= ~0x100u;
        if (gPylonMoveMark2 != nullptr) {
            tag_sTrackBrace* end = gPylonMoveMark1;
            tag_sTrackBrace* walk;
            if (end->mNumber > gPylonMoveMark2->mNumber) {
                walk = gPylonMoveMark2;
            } else {
                walk = end;
                end = gPylonMoveMark2;
            }
            while (walk != end) {
                walk->mFlags &= 0xFEFFBFFFu;
                walk = walk->mPrev;
            }
            generate = true;
        }
        gPylonMoveMark1 = nullptr;
        gPylonMoveMark2 = nullptr;
        gExtrudeTextureKind = 0;
        if (generate) FlagGenerateTrack(gCoasterEditInstance, 0);
        if (gPylonMoveBrace->mData == gPylonMoveOrigData) gPylonMoveOrigData = nullptr;
        MoveBrace(gCoasterEditInstance, gPylonMoveBrace, gPylonMoveOrigData, static_cast<uint32_t>(gPylonMoveX),
                  static_cast<uint32_t>(gPylonMoveZ), nullptr, nullptr, 1, 0);
        LLCC_AbortPylonMove();
        LockPointedAtRide(0);
        gPylonMoveBrace = nullptr;
        gPylonMoveOrigData = nullptr;
        if (gActionRecorder.mRecording != 0) {
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0;
        }
    }
}

// Unit v5314: SetCoasterEdit 0x10058758, GetPylonFromBraceMesh 0x100587fc, GetNthBraceMesh 0x10058888, HandleCoasterButtonClick 0x10058510.

// 0x100585f4 (296 bytes): the overload that takes the instance.
void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag);

namespace {

inline uint32_t Word(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

}  // namespace

// 0x10058510 (176 bytes): event bit 2 releases the buttons (only those in 0x1ff), bit 1 presses, bit 4 clears all.
void HandleCoasterButtonClick(uint32_t buttons, uint32_t event) {
    if ((event & 2) != 0) {
        if ((buttons & 0x1ff) != 0) {
            DisableCurrentPylonEditState();
            gCoasterEditDown = gCoasterEditDown & ~buttons;
        }
    } else if ((event & 1) != 0) {
        HandleCoasterButtonDown(buttons);
    } else if ((event & 4) != 0) {
        DisableCurrentPylonEditState();
        gCoasterEditEnabled = 0;
        gCoasterEditDown = 0;
    }
    if (buttons != 0) {
        GetCoasterEditAvailableButtons(nullptr);
        gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    }
}
// 0x10058758 (124 bytes): the coaster object's word 0x1c is the entrance id. Does nothing while a recording runs.
void SetCoasterEdit(void* coaster, int32_t flag) {
    tag_sCoasterInstance* instance = nullptr;
    if (coaster != nullptr) instance = FindCoasterInstanceFromEntranceID(Word(coaster, 0x1c));
    if (gActionRecorder.mRecording == 0) {
        if (flag != 0)
            SetCoasterEdit(instance, 1);
        else
            SetCoasterEdit(static_cast<tag_sCoasterInstance*>(nullptr), 0);
    }
}

// Unit v5943: CalculateBraceCost 0x10058924, SetCoasterEdit(tag_sCoasterInstance*, int) 0x100585f4, CoasterAcceptLeftMouseClick 0x10057f80.

// 0x10067254 (12 bytes): the ride the pointer is over.
tag_sRideAnimSet* GetPointedAtRide();

// 0x100585f4 (296 bytes): enters (instance and flag) or leaves the track editor for a coaster.
void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag) {
    bool enter = instance != nullptr && flag != 0;
    gActionRecorder.RecordAction(0x44, static_cast<uint16_t>(enter ? instance->mObjectInstanceID : 0));
    if (enter) {
        if (gCoasterEditInstance == nullptr) {
            static_cast<CObject*>(instance->mCObject)->CloseObject();
            if ((instance->mBlock3C & 1) == 0 || (instance->mBlock3C & 2) != 0)
                SetCoasterState(instance, 1);
            else
                SetCoasterState(instance, 2);
        }
        SetCurrentEditCoaster(instance);
    } else if (gCoasterEditInstance != nullptr) {
        DisableCurrentPylonEditState();
        gCoasterEditEnabled = 0;
        gCoasterEditDown = 0;
        FlagGenerateTrack(gCoasterEditInstance, 0);
        tag_sCoasterInstance* edit = gCoasterEditInstance;
        SetCurrentEditCoaster(nullptr);
        if ((edit->mState & 2) != 0) static_cast<CObject*>(edit->mCObject)->Reopen();
    }
}

// 0x100587fc (92 bytes): answers the brace list (a 32-bit address) of ride set `id`, 0 when the set has no braces.
// *out is the list's count, less one when the list's flag 0x20 is set. The set's word 0xd4 points at a block whose word 0x100 is the list.
sBraceMeshList* GetPylonFromBraceMesh(uint32_t id, uint32_t* out) {
    tag_sRideAnimSet* set = gRideAnimSets[id];
    if ((set->mFlags & 0x80000) == 0) {
        *out = 0;
        return nullptr;
    }
    sBraceMeshList* list = static_cast<sBraceMeshBlock*>(set->mBraceBlock)->mList;
    if ((list->mFlags & 0x20) != 0)
        *out = list->mCount - 1;
    else
        *out = list->mCount;
    return list;
}

// 0x10058888 (112 bytes): the n-th brace mesh (the entry's pointer at 0x70) of a list; with flag 0x20 the entries flagged 0x2000 are skipped.
void* GetNthBraceMesh(sBraceMeshList* list, uint32_t n) {
    if ((list->mFlags & 0x20) == 0) return list->mEntries[n]->mMesh;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < list->mCount; i++) {
        sBraceMeshEntry* entry = list->mEntries[i];
        if ((entry->mFlags & 0x2000) == 0) {
            if (n == seen) return entry->mMesh;
            seen++;
        }
    }
    return nullptr;
}

// 0x10058924 (432 bytes): the cost of a brace: its pylon's base cost (when `a`), plus the distance to the next brace (or to the given point) and
// the height of its interval, each times the pylon control's price. The pylon control is the type's A one when the pylon's flags say so.
float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    if (brace == nullptr) return 0.0f;
    sLinkPylon* pylon = reinterpret_cast<sLinkPylon*>(const_cast<sTrackBraceData*>(brace->mData));
    if (pylon == nullptr) return 0.0f;
    sPylonControl* control = pylon->mControl;
    if (control == nullptr) return 0.0f;
    float x = d != nullptr ? *d : brace->mNext->mX4C;
    float z = e != nullptr ? *e : brace->mNext->mZ54;
    sPylonControl* other = pylon->mOther != nullptr ? pylon->mOther->mControl : nullptr;
    sPylonControl* typeA = const_cast<sPylonControl*>(&reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mPylonsA);
    float cost = 0.0f;
    float height = GetHeightForInterval(brace->mFE8w, control) / 10.0f;
    float distance;
    if (b != nullptr) {
        float dz = *c - z;
        float dx = *b - x;
        float dz2 = dz * dz;
        distance = static_cast<float>(std::sqrt(ppc_fmadd(dx, dx, dz2))) / 10.0f;
    } else {
        float dz = brace->mZ54 - z;
        float dx = brace->mX4C - x;
        float dz2 = dz * dz;
        distance = static_cast<float>(std::sqrt(ppc_fmadd(dx, dx, dz2))) / 10.0f;
    }
    if (a != 0) cost = cost + control->mF5C;
    if ((control->mFlags & 0xc000) != 0) {
        if (other == nullptr || (other->mFlags & 0xc000) == 0) control = typeA;
        if ((control->mFlags & 0x4000) != 0 && (brace->mNext->mFlags & 0x80) == 0) control = typeA;
    }
    cost = ppc_fmadds(control->mF60, distance, cost);
    cost = ppc_fmadds(control->mF64, height, cost);
    return cost;
}

namespace {

// The brace of the editor's coaster whose ride is `ride` (the first single-brace-free pylon node's brace), or NULL. The edit coaster is not tested.
tag_sTrackBrace* FindBraceOfRide(tag_sRideAnimSet* ride) {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance->mLastBrace != nullptr) {
        for (sCoasterPylon* node = instance->mPylons; node != nullptr; node = node->mNext) {
            if ((node->mFlags & 1) != 0) continue;
            for (uint32_t i = 0; i < node->mCount; i++) {
                tag_sTrackBrace* brace = node->mBraces[i];
                if (gRideAnimSets[brace->mWord70] == ride) return brace;
            }
        }
    }
    return nullptr;
}

}  // namespace

// 0x10057f80 (1,372 bytes): the left button of the track editor goes down (down != 0) or up, with the pylon tool in gCoasterWord16E0's low byte.
void CoasterAcceptLeftMouseClick(int32_t down) {
    if (gActionRecorder.mRecording != 0) return;
    uint32_t result = 0;
    tag_sRideAnimSet* ride = GetPointedAtRide();
    switch (gCoasterWord16E0 & 0xff) {
    case 2:
        if (down != 0 && gRideHighlightKeep == 0) {
            tag_sTrackBrace* brace = FindBraceOfRide(ride);
            gCoasterDragBrace = brace;
            result = StartMovePylon(brace) ? 1 : 0;
        }
        break;
    case 4:
        if (!((down != 0 && (gCoasterInputFlags & 4) != 0) || (down == 0 && (gCoasterInputFlags & 4) == 0))) {
            if (down != 0) gCoasterDragBrace = FindBraceOfRide(ride);
            tag_sCoasterInstance* instance = gCoasterEditInstance;
            if (instance != nullptr && instance->mF40 != 0 && gCoasterDragBrace != nullptr && (gCoasterDragBrace->mData->mFlags & 2) == 0) {
                if ((gCoasterDragBrace->mFlags & 0x40) != 0)
                    ToggleLoopTurnPylonState();
                else
                    ToggleBankInclinePylonState();
            }
        }
        break;
    case 8:
        if (down != 0 && (gCoasterInputFlags & 1) == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleLoftPylonState();
        } else if (down == 0 && (gCoasterInputFlags & 1) != 0) {
            ToggleLoftPylonState();
        }
        break;
    case 0x10:
        if (!((down != 0 && (gCoasterInputFlags & 2) != 0) || (down == 0 && (gCoasterInputFlags & 2) == 0))) {
            if (down != 0) gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleRotatePylonState();
        }
        break;
    case 0x20:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleSwapUpPylonState();
        }
        break;
    case 0x40:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleSwapDownPylonState();
        }
        break;
    case 0x80:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            DoDeleteMultiPylons();
        }
        break;
    default:
        break;
    }
    (void)result;   // the original leaves StartMovePylon's answer in r3; the headers of the callers declare it void
}

// 0x10058b30
int32_t IsCoasterLoftMode() { return static_cast<int32_t>(gCoasterInputFlags & 1); }

// 0x10058b68
void SetPlaceNormalPylon() { SetPylonType(1); }

// 0x10058bb8
void SetPlaceSpecialPylon() { SetPylonType(2); }

// Unit v5261: the layout commands that act on the editor's coaster (LayoutSetMovePylon and its neighbours, 0x10058c08 .. 0x1005901c).

// Callees declared here (not in a header): DoDeleteMultiPylons 0x10053f98; SetCoasterEdit 0x100585f4 takes the instance.
void DoDeleteMultiPylons();
void SetCoasterEdit(void* coaster, int32_t flag);

namespace {

// The braces' pylon record sits at the brace's +0x100; its table of brace pointers starts at +0x30 (a pointer each: 4 bytes on the Mac).
tag_sTrackBrace* PylonTableEntry(tag_sTrackBrace* brace, int32_t wordOffset) {
    uint32_t index;
    std::memcpy(&index, reinterpret_cast<const uint8_t*>(brace) + 4, 4);   // the brace's word +4
    tag_sTrackBrace* const* table = reinterpret_cast<tag_sTrackBrace* const*>(reinterpret_cast<const uint8_t*>(brace->mData) + 0x30);
    return table[index + wordOffset];
}

sCoasterTrackParams* EditParams() {
    return reinterpret_cast<sCoasterTrackParams*>(&gCoasterEditInstance->mBlock3C);   // +0x3c
}

}  // namespace

// 0x10058c08 (76 bytes): swaps the brace with the table entry two places on (offset 0x38 - 0x30).
void LayoutStackPylonUp(uint16_t id) {
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), id);
    ExchangeTrackBraces(brace, PylonTableEntry(brace, 2));
}

// 0x10058c80 (76 bytes)
void LayoutStackPylonDown(uint16_t id) {
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), id);
    ExchangeTrackBraces(brace, PylonTableEntry(brace, 0));
}

// 0x10058cf8 (120 bytes)
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b) {
    gCoasterDragBrace = GetTrackBrace(EditParams(), a);
    DoDeleteMultiPylons();
    gCoasterDragBrace = GetTrackBrace(EditParams(), b);
    DoDeleteMultiPylons();
}

// 0x10058da4 (116 bytes); the second argument is unused
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c) {
    (void)a;
    (void)b;
    DisableCurrentPylonEditState();
    gCoasterWord16E0 = 2;
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), c);
    gCoasterDragBrace = brace;
    StartMovePylon(brace);
}

// 0x10058e48 (76 bytes)
void LayoutRegenerateTrack() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance->mFlags & 1) {
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        GenerateTrack(instance);
    }
}

// 0x10058ec0 (92 bytes)
void LayoutRegenerateTrack(uint32_t id) {
    tag_sCoasterInstance* instance = static_cast<tag_sCoasterInstance*>(gObjectTable[id]);
    if (instance != nullptr && (instance->mFlags & 1)) {
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        GenerateTrack(instance);
    }
}

// 0x10058f4c (same machine code as Pilot::move)
void LayoutDeleteLastTrackBrace() { DeleteLastTrackBrace(); }

// 0x10058fa0 (76 bytes)
void LayoutSetCoasterEditOld(uint32_t entrance) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    SetCoasterEdit(instance, entrance != 0 ? 1 : 0);
}

// 0x1005901c (88 bytes)
void LayoutSetCoasterEdit(uint32_t id) {
    void* instance = id != 0 ? gObjectTable[id] : nullptr;
    SetCoasterEdit(instance, id != 0 ? 1 : 0);
}

// 0x100590a0
void LayoutRecordCoasterSpeed(tag_sCoasterInstance* instance, uint32_t speed) {
    gActionRecorder.RecordAction(static_cast<RecordedAction>(0x45), static_cast<uint16_t>(instance->mObjectInstanceID),
                                 static_cast<uint16_t>(speed));
}

// LayoutReplayCoasterSpeed 0x10059120 (80 B): sets the coaster's speed, then marks its object (if any) with flag 0x8000.

void LayoutReplayCoasterSpeed(uint32_t instance, uint32_t speed);

void LayoutReplayCoasterSpeed(uint32_t instance, uint32_t speed) {
    CoasterSetSpeed(instance, speed);
    uint32_t* object = static_cast<uint32_t*>(gObjectTable[instance]);   // no bounds check, like the original
    if (object != nullptr) *object |= 0x8000;
}
