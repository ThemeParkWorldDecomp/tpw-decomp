// The staff status panel's message functions (main app, UI): StaffStatusMsg 0x101c4f8c (912 bytes) and OneEditMsg 0x101c56bc (388 bytes).
#include <cstdint>
#include <new>

#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

namespace {

// The Mac calls a virtual member of a panel through its table at +0x20: the slot holds an arena offset of a block holding the function.
void CallSlot(UIStatusPanelFields* fields, uint32_t slot, void* self) {
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    (*entry.get())(self);
}

}  // namespace

int32_t StaffStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffStatusPanel* panel = &gStaffStatusPanel;
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);

    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (static_cast<uint32_t>(a - 0x80000) == 0x80) panel->UpdateStatusWindow();
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc0));
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd4));
        }
    } else if (message == 0x100) {
        CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
        if (a == 0x50e) {
            panel->SetSelected();
            staff->PickMeUp();
            CallSlot(fields, 0x34, panel);
            return 0;
        } else if (a < 0x50e) {
            if (a == 0x50b) {
                CreateAllStaffPanel(staff->GetStaffType(), gStaffRoomPanelThing);
                return 0;
            } else if (a < 0x50b) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else if (a < 0x50d) {
                panel->FireMe();
                return 0;
            } else {
                reinterpret_cast<UIStatusPanel*>(panel)->GetPreviousObject();
            }
        } else if (a == 0x521) {
            ShowStaffPanel(gStaffRoomPanelThing);
            return 0;
        } else if (a < 0x521) {
            if (a == 0x510) {
                bool handled;
                if (b == 1) {
                    panel->SetSelected();
                    CMMPathDraw* pathDraw = static_cast<CMMPathDraw*>(::operator new(0x14));
                    if (pathDraw != nullptr) new (pathDraw) CMMPathDraw(static_cast<CELL_MODES>(0x36));
                    if (gMouse == nullptr) {
                        CMouse* mouse = static_cast<CMouse*>(::operator new(1));
                        if (mouse != nullptr) {
                            CMMDefault* defaultMode = static_cast<CMMDefault*>(::operator new(8));
                            if (defaultMode != nullptr) new (defaultMode) CMMDefault();
                            mouse->AssignMouseMode(defaultMode);
                            gMouseWord1E0 = -1;
                            gMouseWord1E4 = -1;
                        }
                        gMouse = mouse;
                    }
                    gMouse->AssignMouseMode(pathDraw);
                    CallSlot(fields, 0x34, panel);
                    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc1));
                    handled = true;
                } else {
                    staff->CancelPatrolArea();
                    handled = false;
                }
                if (handled) return 0;
            } else if (a < 0x510) {
                reinterpret_cast<UIStatusPanel*>(panel)->GetNextObject();
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
        if (staff != nullptr) {
            InterfaceWindow* button = fields->mWindow->GetWindow(0x50e);
            if (staff->CanBePickedUp() != 0) {
                button->Enable(1);
            } else {
                button->Enable(0);
            }
            UIMain_UpdateSpritePreview();
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

int32_t OneEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        UI_PostMessage(window, 0x100, -1, 0);
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                CStaff* staff = static_cast<CStaff*>(window->getExtraParam(0));
                staff->SetName(gStaffEditName);
                UIStatusPanelFields* fields = static_cast<UIStatusPanelFields*>(window->getExtraParam(1));
                CallSlot(fields, 0xc, fields);
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            } else if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            static_cast<UIStatusPanel*>(window->getExtraParam(1))->EditNamePanelGone();
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x804) {
        UI_SendMessage(window, 4, 0, 0);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
