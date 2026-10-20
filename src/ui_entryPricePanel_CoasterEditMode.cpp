// ui_entryPricePanel.cpp (main app), unit v5246: the coaster edit mode's entry and the panel's special button.
//   CoasterEditMode 0x1019c584 (1308 bytes), WhichSpecialButton 0x1019c3e8 (368 bytes)
#include <cstdint>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

void* gCoasterEditObject;      // 0x1039482c (name invented): the coaster being edited
uint8_t gCoasterEditBusy;      // 0x10394828 (name invented): nonzero keeps CoasterEditMode from starting

namespace {

// The byte at +0x4c of the Mac's CObject (read raw: the host's CObject is laid out differently).
uint8_t ObjectByte4c(void* object) { return *(reinterpret_cast<uint8_t*>(object) + 0x4c); }

// The original builds the mouse when there is none: a CMouse with a default mode assigned, both words set to -1. Inlined at three places.
void MakeMouseIfNone() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

}  // namespace

// 0x1019c3e8: shows or hides the panel's special button (child 0x20 of child 0x18) for the coaster being edited.
void WhichSpecialButton() {
    InterfaceWindow* button = gCoasterPanel->GetWindow(0x18)->GetWindow(0x20);
    char* names = const_cast<char*>("cb_trough\0cb_loop");   // the Mac's strings at 0x1024e00a and 0x1024e014
    if (ObjectByte4c(gCoasterEditObject) >= 1) {
        uint32_t buttons;
        GetCoasterEditAvailableButtons(&buttons);
        if ((buttons & 0x200) != 0) {
            button->Enable(1);
            button->SetDrawObject(DrawObjectLibrary::FindDrawObject(names));
            button->mHelpText = 0x134;
            button->mF90 = 0x134;
        } else {
            button->Enable(1);
            button->SetDrawObject(DrawObjectLibrary::FindDrawObject(names + 10));
            button->mHelpText = 0x132;
            button->mF90 = 0x132;
        }
        if ((buttons & 2) != 0)
            button->Enable(1);
        else
            button->Enable(0);
    } else {
        button->SetDrawObject(nullptr);
        button->Enable(0);
    }
}

// 0x1019c584
void CoasterEditMode(void* object) {
    if (gCoasterEditMode != 0 && gCoasterEditObject == object) return;
    if (gCoasterEditBusy != 0) return;
    gCoasterEditObject = object;
    UI_SetSelectedObject(object);
    LLCM_SetCurrentObject(true);
    if (gActionRecorder.mRecording == 0) {
        RemoveSlideoutPanel(1);
        LinkSlideoutPanel(gCoasterPanel, 0);
    }
    gCoasterEditMode = 1;
    gCoasterPanel->CaptureKb(0);
    if (IsTrackClosed(object) == 0) {
        LLCM_SetCurrentObject(false);
        CMMPathDraw* pathDraw = new CMMPathDraw(static_cast<CELL_MODES>(0x15));
        MakeMouseIfNone();
        gMouse->AssignMouseMode(pathDraw);
        LLCC_SystemSetRideTrack(3);
    } else {
        SetCoasterEdit(gCoasterEditObject, 1);
        MakeMouseIfNone();
        int32_t mode = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
        if (mode != 8) {
            CMMCoaster* coaster = new CMMCoaster;
            MakeMouseIfNone();
            gMouse->AssignMouseMode(coaster);
        }
        UI_SetGameWindowCursor(0xc);
        UI_SetSelectedObject(object);
        LLCM_SetCurrentObject(true);
    }
    if (gCoasterEditMode != 0) {
        uint32_t buttons;
        GetCoasterEditAvailableButtons(&buttons);
        InterfaceWindow* panel = gCoasterPanel->GetWindow(0x18);
        panel->GetWindow(0x1f)->Enable((buttons & 1) == 1);
        int16_t special = 0;
        if ((buttons & 2) == 2 && ObjectByte4c(gCoasterEditObject) >= 1) special = 1;
        panel->GetWindow(0x20)->Enable(special);
        panel->GetWindow(0x1b)->Enable((buttons & 4) == 4);
        panel->GetWindow(0x19)->Enable((buttons & 8) == 8);
        panel->GetWindow(0x1a)->Enable((buttons & 0x10) == 0x10);
        panel->GetWindow(0x1c)->Enable((buttons & 0x20) == 0x20);
        panel->GetWindow(0x21)->Enable((buttons & 0x40) == 0x40);
        panel->GetWindow(0x1e)->Enable((buttons & 0x80) == 0x80);
        panel->GetWindow(0x1d)->Enable((buttons & 0x100) == 0x100);
        GetCoasterEditDefaultButtons(&buttons);
        WhichCheckButtonsAreDown(buttons);
    }
    WhichSpecialButton();
    gCoasterEditMode = 1;
}
