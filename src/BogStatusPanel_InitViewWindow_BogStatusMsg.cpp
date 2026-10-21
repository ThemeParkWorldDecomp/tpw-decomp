// Unit v5699: BogStatusMsg 0x1019a9b0, __sinit_ui_bogStatus_cpp 0x1019ad70, CoasterPlaceMouseMode 0x1019b018,
// WhichCheckButtonsAreDown 0x1019b120.
#include <cstdint>
#include <cstring>
#include <new>
#include "CellBase.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"

// 0x1019a9b0 (628 bytes): the bog status panel's window handler. Everything not answered falls to DefaultStatusPanelMsg.
int32_t BogStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    BogStatusPanel* panel = &gBogStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (a == 0x80080) panel->UpdateStatusWindow();
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12b));
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd5));
        }
    } else if (message == 0x100) {
        if (a == 0x15bb7) {
            base->GetNextObject();
        } else if (a < 0x15bb7) {
            if (a == 0x15bb4) {
                base->RequestMaintenance(b != 1);
                return 0;
            }
            if (a < 0x15bb4) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else {
                if (a < 0x15bb6) {
                    base->EraseObject();
                    return 0;
                }
                base->GetPreviousObject();
            }
        } else {
            if (a == 0x15bc3) {
                base->Move();
                return 0;
            }
            if (a < 0x15bc3) {
                if (a >= 0x15bc2) {
                    CreateAllItemsPanel(kUIObjFeature, gStaffRoomPanelThing);
                    return 0;
                }
            } else if (a < 0x15bc5) {
                base->CloseObject(b == 1 ? 1 : 0);
                panel->UpdateStatusWindow();
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        UIMain_TickRidePreview();
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x1019ad70 (76 bytes): the static constructor of gBogStatusPanel (0x10394800).
void __sinit_ui_bogStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gBogStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gBogStatusPanel));
    fields->mVtable.set(BogStatusPanelTable());
    fields->mMode = 0x3ea;
}

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
