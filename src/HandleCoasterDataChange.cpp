// TPConfig.cpp (main app): the coaster editor's data change and the loft drag toggle. Unit v5054.
//   HandleCoasterDataChange 0x10052318, ToggleLoftPylonState 0x10052ac4
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "World.h"

float gCoasterVerticalFloor;   // 0x1029ce80

namespace {

// The runtime's double to unsigned conversion (0x10237cd8) for a value in range.
uint32_t FloatToUInt(float value) { return static_cast<uint32_t>(static_cast<int64_t>(value)); }

// The brace's pylon record is a sCoasterPylon on the host (native pointers): its control is the word at +0x18.
sPylonControl* ControlOf(const sTrackBraceData* pylon) { return reinterpret_cast<const sCoasterPylon*>(pylon)->mControl; }

}  // namespace

// 0x10052318: flags bit 0 the loft changed, bit 2 the wobble changed; b nonzero skips the collision flag.
void HandleCoasterDataChange(uint32_t flags, tag_sTrackBrace* brace, tag_sCoasterInstance* instance, int32_t a, int32_t b) {
    (void)a;
    const sTrackBraceData* pylon = brace->mData;
    sPylonControl* control = ControlOf(pylon);
    if ((flags & 1) != 0) {
        if (brace == instance->mPreviewBrace) {
            float f = brace->mFE8w;
            if (f < 0.0f) {
                f = 0.0f;
            } else if (f > 1.0f) {
                f = 1.0f;
            }
            if ((control->mFlags & 0x4000) == 0) instance->mF118 = GetHeightForInterval(f, control);
        }
        if ((instance->mFlags & 0x800) != 0) DoImmediatePhysicsFeedback(instance);
    } else if ((flags & 4) != 0 && (brace->mFlags & 0x40) == 0 && brace == instance->mPreviewBrace) {
        instance->mF11C = brace->mFD8;
    }
    FlagGenerateTrack(instance, 1);
    if ((flags & 1) != 0) {
        SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(instance->mType)), brace);
        UpdatePylonStackMeshes(control, const_cast<tag_sPylon*>(pylon), 0, 0);
    }
    if (b == 0) FlagCollisionUpdateTrackAroundBrace(brace);
}

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
