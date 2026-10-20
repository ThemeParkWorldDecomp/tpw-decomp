// CoasterPanelMsg 0x1019b2b8: the message handler of the coaster edit panel (gCoasterPanel).
#include "ui_main.h"

#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCoaster.h"
#include "keyboard.h"

namespace {

// The same four steps the original repeats inline: a mouse with a default mode, once.
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

void PathDrawMouse(int32_t cellMode) {
    CMMPathDraw* pathDraw = new CMMPathDraw(static_cast<CELL_MODES>(cellMode));
    EnsureMouse();
    gMouse->AssignMouseMode(pathDraw);
}

void CoasterMouse() {
    EnsureMouse();
    int32_t mode = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
    if (mode != 8) {
        CMMCoaster* coaster = new CMMCoaster;
        EnsureMouse();
        gMouse->AssignMouseMode(coaster);
    }
}

void Advisor(int32_t message) { UIMain_AdvisorMessage(static_cast<AdvisorMessage>(message)); }
void CancelAdvisor(int32_t message) { UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(message)); }

// Button 1 (control 0x1f)
void ButtonPathStraight(bool down) {
    if (down) {
        LLCM_SetCurrentObject(false);
        PathDrawMouse(0x15);
        LLCC_SystemSetRideTrack(3);
        HandleCoasterButtonClick(1, 1);
        Advisor(0x125);
    } else {
        HandleCoasterButtonClick(1, 2);
    }
}

// Button 2 (control 0x20)
void ButtonPathSpecial(bool down) {
    if (down) {
        LLCM_SetCurrentObject(false);
        PathDrawMouse(0x15);
        LLCC_SystemSetRideTrack(3);
        HandleCoasterButtonClick(2, 1);
        uint32_t buttons;
        GetCoasterEditAvailableButtons(&buttons);
        if ((buttons & 0x200) != 0)
            Advisor(0x127);
        else
            Advisor(0x126);
    } else {
        HandleCoasterButtonClick(2, 2);
    }
}

// Button 4 (control 0x1b)
void ButtonPathFour(bool down) {
    if (down) {
        LLCM_SetCurrentObject(false);
        PathDrawMouse(0x16);
        LLCC_SystemSetRideTrack(3);
        HandleCoasterButtonClick(4, 1);
        Advisor(0x121);
    } else {
        HandleCoasterButtonClick(4, 2);
    }
}

// The buttons that put the coaster mouse mode on with a cursor.
void ButtonCursor(bool down, int16_t cursor, uint32_t button) {
    if (down) {
        CoasterMouse();
        UI_SetGameWindowCursor(cursor);
        HandleCoasterButtonClick(button, 1);
    } else {
        HandleCoasterButtonClick(button, 2);
    }
}

void ButtonCursor40(bool down) {
    if (down) {
        CoasterMouse();
        UI_SetGameWindowCursor(0xa);
        HandleCoasterButtonClick(0x40, 1);
        Advisor(0x124);
    } else {
        HandleCoasterButtonClick(0x40, 2);
    }
}

}  // namespace

int32_t CoasterPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101) {
        uint32_t index = static_cast<uint32_t>(a) - 0x19u;
        if (index <= 8) {
            bool down = (b == 1);
            switch (index) {
                case 0: ButtonCursor(down, 0xe, 8); break;       // control 0x19
                case 1: ButtonCursor(down, 0xc, 0x10); break;    // control 0x1a
                case 2: ButtonPathFour(down); break;             // control 0x1b
                case 3: ButtonCursor(down, 0xd, 0x20); break;    // control 0x1c
                case 4: ButtonCursor(down, 0xf, 0x100); break;   // control 0x1d
                case 5: ButtonCursor(down, 0x10, 0x80); break;   // control 0x1e
                case 6: ButtonPathStraight(down); break;         // control 0x1f
                case 7: ButtonPathSpecial(down); break;          // control 0x20
                case 8: ButtonCursor40(down); break;             // control 0x21
            }
        }
    } else if (message < 0x101) {
        if (message == 0x1d) {
            gCoasterEditBusy = (a == 1) ? 1 : 0;
        } else if (message < 0x1d && message == 0x14) {
            gCoasterPanel = nullptr;
        }
    } else {
        int16_t key = static_cast<int16_t>(a);
        int16_t modifiers = static_cast<int16_t>(static_cast<uint8_t>(static_cast<uint32_t>(b) >> 16));
        if (message == 0x1000b) {
            if (gKeyTableCamera->KeyUp(key, modifiers) == 0) gKeyTableCoaster->KeyUp(key, modifiers);
        } else if (message < 0x1000b) {
            if (message >= 0x1000a) {
                if (gKeyTableCamera->KeyDown(key, modifiers) == 0) gKeyTableCoaster->KeyDown(key, modifiers);
            }
        } else if (message == 0x1001d8) {
            if (gCoasterEditMode != 0) {
                HandleCoasterButtonClick(0, 4);
                gCoasterEditObject = nullptr;
                if (LLCC_SystemGetMode() != 3) {
                    EnsureMouse();
                    int32_t mode = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
                    if (mode != 1) {
                        CMMDefault* fresh = new CMMDefault;
                        EnsureMouse();
                        gMouse->AssignMouseMode(fresh);
                        LLCC_SystemSetMode(static_cast<CELL_MODES>(0), true);
                        LLCC_SystemSetMode(static_cast<CELL_MODES>(0), true);
                    }
                }
                SetCoasterEdit(nullptr, 0);
                gCoasterEditMode = 0;
                UIMain_DecideKbCapture();
            }
            CancelAdvisor(0x125);
            CancelAdvisor(0x127);
            CancelAdvisor(0x126);
            CancelAdvisor(0x121);
            CancelAdvisor(0x124);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
