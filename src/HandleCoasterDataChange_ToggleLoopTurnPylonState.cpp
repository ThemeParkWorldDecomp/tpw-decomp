// The editor's drag starts for a pylon: loop turn 0x10052e5c, rotate 0x100530a8, bank and incline 0x100532f4.
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCamera.h"
#include "TPCoaster.h"

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
