#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

// 0x100565cc
void UIHandleEndLoop() {
    UIInterface* ui = &gUIInterface;
    GetCoasterEditAvailableButtons(nullptr);
    SetCoasterButtonState(1, 2);
    ui->ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    ui->ChangeCoasterButtonsDown(gCoasterEditDown);
}

// 0x10056548
void UIHandleMidLoop() {
    UIInterface* ui = &gUIInterface;
    GetCoasterEditAvailableButtons(nullptr);
    ui->ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    ui->ChangeCoasterButtonsDown(gCoasterEditDown);
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
