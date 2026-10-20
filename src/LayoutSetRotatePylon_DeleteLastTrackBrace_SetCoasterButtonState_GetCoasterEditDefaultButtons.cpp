#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

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
