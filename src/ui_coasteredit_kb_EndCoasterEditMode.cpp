// ui_entryPricePanel.cpp, main app: leaving the coaster edit mode, and enabling the edit panel's buttons.
//   EndCoasterEditMode 0x1019cad0 (516 bytes), CoasterButtonsEnabled 0x1019cd6c (576 bytes)
#include "InterfaceWindow.h"
#include "ui_main.h"
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCoaster.h"

namespace {

// The mouse object the original builds on demand (operator new(1), a default mode, assigned; both words set to -1).
CMouse* MakeMouse() {
    CMouse* mouse = new CMouse();
    CMouseMode* mode = new CMMDefault();
    mouse->AssignMouseMode(mode);
    gMouseWord1E0 = -1;
    gMouseWord1E4 = -1;
    return mouse;
}

}  // namespace

// 0x1019cad0
void EndCoasterEditMode() {
    if (gCoasterEditMode != 0) {
        if (gActionRecorder.mRecording == 0) RemoveSlideoutPanel(0);
        if (gCoasterEditMode != 0) {
            HandleCoasterButtonClick(0, 4);
            gCoasterEditObject = nullptr;
            if (LLCC_SystemGetMode() != 3) {
                if (gMouse == nullptr) gMouse = MakeMouse();
                int32_t mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
                if (mode != 1) {
                    CMouseMode* fresh = new CMMDefault();
                    if (gMouse == nullptr) gMouse = MakeMouse();
                    gMouse->AssignMouseMode(fresh);
                    LLCC_SystemSetMode(0, true);
                    LLCC_SystemSetMode(0, true);
                }
            }
            SetCoasterEdit(nullptr, 0);
            gCoasterEditMode = 0;
            UIMain_DecideKbCapture();
        }
        gCoasterEditMode = 0;
    }
}

// 0x1019cd6c: the window at +0x118 of each button is the Enable table (slot +0x1c)
void CoasterButtonsEnabled(uint32_t buttons) {
    if (gCoasterPanel == nullptr || gCoasterEditMode == 0) return;
    InterfaceWindow* panel = gCoasterPanel->GetWindow(0x18);
    panel->GetWindow(0x1f)->Enable((buttons & 1) == 1);
    int16_t special = 0;
    if ((buttons & 2) == 2 && *(reinterpret_cast<uint8_t*>(gCoasterEditObject) + 0x4c) >= 1) special = 1;
    panel->GetWindow(0x20)->Enable(special);
    panel->GetWindow(0x1b)->Enable((buttons & 4) == 4);
    panel->GetWindow(0x19)->Enable((buttons & 8) == 8);
    panel->GetWindow(0x1a)->Enable((buttons & 0x10) == 0x10);
    panel->GetWindow(0x1c)->Enable((buttons & 0x20) == 0x20);
    panel->GetWindow(0x21)->Enable((buttons & 0x40) == 0x40);
    panel->GetWindow(0x1e)->Enable((buttons & 0x80) == 0x80);
    panel->GetWindow(0x1d)->Enable((buttons & 0x100) == 0x100);
}
