// ui_coasterPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "CellBase.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LLCellSystem.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v5699: BogStatusMsg 0x1019a9b0, __sinit_ui_bogStatus_cpp 0x1019ad70, CoasterPlaceMouseMode 0x1019b018,
// WhichCheckButtonsAreDown 0x1019b120.

// 0x1019b018 (220 bytes): the path drawing mode for the coaster placing, given to the mouse (made first when there is none).
void CoasterPlaceMouseMode() {
    LLCM_SetCurrentObject(false);
    CMMPathDraw* mode = nullptr;
    void* memory = lender_callee::OperatorNew(0x14);
    if (memory != nullptr) mode = new (memory) CMMPathDraw(static_cast<CELL_MODES>(0x15));
    if (gMouse == nullptr) {
        CMouse* mouse = nullptr;
        void* mouseMemory = lender_callee::OperatorNew(1);
        if (mouseMemory != nullptr) {
            mouse = new (mouseMemory) CMouse;
            CMMDefault* defaultMode = nullptr;
            void* defaultMemory = lender_callee::OperatorNew(8);
            if (defaultMemory != nullptr) defaultMode = new (defaultMemory) CMMDefault;
            mouse->AssignMouseMode(defaultMode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(mode);
    LLCC_SystemSetRideTrack(3);
}

// 0x1019b120 (360 bytes): the check button of the coaster panel for the lowest set bit of the mask (in the order 4, 1, 2, 8, 0x10,
// 0x20, 0x40, 0x80, 0x100), and the matching game cursor for the last seven; -1 (none) when no bit is set.
void WhichCheckButtonsAreDown(uint32_t mask) {
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(gCoasterPanel->GetWindow(0x18));
    if (mask & 4) {
        buttons->SetCurrent(0x1b);
    } else if (mask & 1) {
        buttons->SetCurrent(0x1f);
    } else if (mask & 2) {
        buttons->SetCurrent(0x20);
    } else if (mask & 8) {
        buttons->SetCurrent(0x19);
        UI_SetGameWindowCursor(0xe);
    } else if (mask & 0x10) {
        buttons->SetCurrent(0x1a);
        UI_SetGameWindowCursor(0xc);
    } else if (mask & 0x20) {
        buttons->SetCurrent(0x1c);
        UI_SetGameWindowCursor(0xd);
    } else if (mask & 0x40) {
        buttons->SetCurrent(0x21);
        UI_SetGameWindowCursor(0xa);
    } else if (mask & 0x80) {
        buttons->SetCurrent(0x1e);
        UI_SetGameWindowCursor(0xf);
    } else if (mask & 0x100) {
        buttons->SetCurrent(0x1d);
        UI_SetGameWindowCursor(0x10);
    } else {
        buttons->SetCurrent(-1);
    }
}

// CoasterPanelMsg 0x1019b2b8: the message handler of the coaster edit panel (gCoasterPanel).

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

// CoasterPanelEndTrack 0x1019c158 (344 bytes): makes sure the mouse exists, and unless the current mouse mode answers 8 (coaster),
// installs a new CMMCoaster as the mouse mode. The "make the mouse" sequence is inlined twice (the original's lazy gMouse creation).

namespace coaster_end_callee {
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(size); }
}

// The constructors run on memory from the fake-able allocation (placement new keeps the order of the original's calls).
static CMouse* MakeMouse() {
    CMouse* mouse = static_cast<CMouse*>(coaster_end_callee::OperatorNew(1));
    CMMDefault* mode = new (coaster_end_callee::OperatorNew(8)) CMMDefault();
    mouse->AssignMouseMode(mode);
    gMouseWord1E0 = -1;
    gMouseWord1E4 = -1;
    return mouse;
}

void CoasterPanelEndTrack() {
    if (gMouse == nullptr) gMouse = MakeMouse();
    int32_t mode = 0;
    if (gMouseMode != nullptr) mode = gMouseMode->GetMode();
    if (mode != 8) {
        CMMCoaster* coaster = new (coaster_end_callee::OperatorNew(8)) CMMCoaster();
        if (gMouse == nullptr) gMouse = MakeMouse();
        gMouse->AssignMouseMode(coaster);
    }
}

// ui_entryPricePanel.cpp (main): InitCashPanel 0x1019af14, InitCoasterPanel 0x1019c2dc,
// InitControlPanel 0x1019d180 (116 B each); DeInitCashPanel 0x1019afac,
// DeInitCoasterPanel 0x1019c378, DeInitControlPanel 0x1019d21c (68 B each).

void InitCoasterPanel() {
    gCoasterPanel = gUIRootWindow->Build(gCoasterPanelScript, CoasterPanelMsg);
    gCoasterPanel->unlink();
    gCoasterPanel->setExtraParam(0, reinterpret_cast<void*>(2));
    gCoasterPanel->Show(0);
}
void DeInitCoasterPanel() { if (gCoasterPanel != nullptr) delete gCoasterPanel; }

// ui_entryPricePanel.cpp (main app), unit v5246: the coaster edit mode's entry and the panel's special button.
//   CoasterEditMode 0x1019c584 (1308 bytes), WhichSpecialButton 0x1019c3e8 (368 bytes)

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

// ui_entryPricePanel.cpp, main app: leaving the coaster edit mode, and enabling the edit panel's buttons.
//   EndCoasterEditMode 0x1019cad0 (516 bytes), CoasterButtonsEnabled 0x1019cd6c (576 bytes)

namespace {

// The mouse object the original builds on demand (operator new(1), a default mode, assigned; both words set to -1).
CMouse* MakeMouse_2() {
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
                if (gMouse == nullptr) gMouse = MakeMouse_2();
                int32_t mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
                if (mode != 1) {
                    CMouseMode* fresh = new CMMDefault();
                    if (gMouse == nullptr) gMouse = MakeMouse_2();
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

// 0x1019cd00 (64 bytes): both the coaster panel and the edit mode must be set.
void CoasterButtonsDown(uint32_t mask) {
    if (gCoasterPanel != nullptr && gCoasterEditMode != 0) WhichCheckButtonsAreDown(mask);
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

// ui_entryPricePanel.cpp (main app), unit w49: the two small functions of the coaster editor's panel that the keyboard decision calls,
// CoasterEditPanelUp 0x1019cfdc (12 bytes) and CoasterEditPanelGetKb 0x1019d014 (48 bytes), and the two globals they read.

int32_t gCoasterEditMode;
InterfaceWindow* gCoasterPanel;

int32_t CoasterEditPanelUp() { return gCoasterEditMode; }

void CoasterEditPanelGetKb() { gCoasterPanel->CaptureKb(0); }
