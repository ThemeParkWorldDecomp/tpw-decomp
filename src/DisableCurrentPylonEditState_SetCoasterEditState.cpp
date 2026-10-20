#include <cstdint>

#include "TPCoaster.h"
#include "UIInterface.h"
#include "ui_main.h"

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
