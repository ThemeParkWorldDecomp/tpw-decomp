// Pylon editing setters of the editor's coaster (TPConfig.cpp range 0x10055e0c..0x10056500).
#include <cstddef>
#include <cstdint>

#include "TPCoaster.h"
#include "keytable_actions.h"

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
