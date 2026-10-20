#include "ui_controls.h"
#include "ui_main.h"
#include "ThingReceivers.h"

// 0x101bbd78 (780 bytes): the handler of the ride status panel's window. Messages not handled here go to DefaultStatusPanelMsg.
int32_t RideStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    RideStatusPanel* panel = &gRideStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message != 0x1d) {
        if (message < 0x1d) {
            if (message == 0x10) {
                if (a == 0x80080) panel->UpdateStatusWindow();
            } else if (message < 0x10) {
                if (message == 5) {
                    panel->Remove();
                    return 1;
                }
            } else if (message == 0x15) {
                if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
            } else if (message < 0x15 && message >= 0x14) {
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x129));
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xcf));
            }
        } else if (message == 0x100) {
            if (a == 0x3e2b) {
                base->EraseObject();
                return 0;
            }
            if (a < 0x3e2b) {
                if (a == 0x3e27) {
                    base->GetNextObject();
                } else if (a < 0x3e27) {
                    if (a >= 0) {
                        if (a >= 0x3e26) base->GetPreviousObject();
                    } else if (a >= -2) {
                        panel->Remove();
                        return 0;
                    }
                } else {
                    if (a == 0x3e29) {
                        CreateAllItemsPanel(kUIObjRide, gStaffRoomPanelThing);
                        return 0;
                    }
                    if (a > 0x3e29) {
                        panel->Edit(1);
                        return 0;
                    }
                }
            } else {
                if (a == 0x3e36) {
                    if (b == 1) base->RequestMaintenance(false);
                    else base->RequestMaintenance(true);
                    return 0;
                }
                if (a < 0x3e36) {
                    if (a == 0x3e34) {
                        panel->Edit(0);
                        return 0;
                    }
                    if (a > 0x3e34) {
                        panel->ShowUpgradePanel();
                        return 0;
                    }
                    if (a < 0x3e2d) {
                        base->Move();
                        return 0;
                    }
                } else if (a == 0x3e38) {
                    if (b == 1) base->CloseObject(1);
                    else base->CloseObject(0);
                    panel->UpdateStatusWindow();
                } else if (a < 0x3e38) {
                    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GoFirstPerson(0);
                    panel->mOps->Slot11(panel);
                    return 0;
                }
            }
        } else if (message < 0x100) {
            if (message < 0x1f) UIMain_TickRidePreview();
        } else if (message == 0x800) {
            panel->DoDial(a, b);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}
