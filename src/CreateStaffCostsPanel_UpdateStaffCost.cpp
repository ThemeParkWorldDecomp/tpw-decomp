// UpdateStaffCost 0x101be030 (412 bytes), StaffCostsPanelMsg 0x101be1f4 (524 bytes), ShopStatusMsg 0x101bdb8c (796 bytes).
#include <cstdint>

#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_iterators.h"
#include "ui_main.h"

// The five cost words of the staff headquarters' training block (Mac +0x0c of the headquarters; the flag at +0x14 of the block).
// Row ids: 0x2e7 word 0, 0x2ec word 1, 0x2ea word 2, 0x2eb word 3, 0x2ed word 4; any other id changes nothing.
void UpdateStaffCost(int32_t id, int32_t value) {
    int32_t index;
    switch (id) {
        case 0x2e7: index = 0; break;
        case 0x2ec: index = 1; break;
        case 0x2ea: index = 2; break;
        case 0x2eb: index = 3; break;
        case 0x2ed: index = 4; break;
        default: return;
    }
    CTrainingCentre& block = gGameWorld->GetStaffHQ()->mTraining;
    block.mF00[index] = value < 0 ? 0 : value;
    if (static_cast<int32_t>(block.mF00[index]) > 0) block.mF14 = 1;
}

int32_t StaffCostsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 5) {
        RemoveStaffCostsPanel();
        return 1;
    }
    if (message == 0x14) {
        gStaffCostsPanel = nullptr;
    } else if (message == 0x100) {
        switch (a) {
            case -1: RemoveStaffCostsPanel(); return 0;
            case 0x2db: CreateEntryPricePanel(); return 0;
            case 0x2dc: CreateFinancePanel(); return 0;
            case 0x2dd: CreateLoansPanel(); return 0;
            default: break;
        }
    } else if (message == 0x800) {
        UpdateStaffCost(a, b);
        InterfaceWindow* summary = gStaffCostsPanel->GetWindow(0x2de);
        if (summary != nullptr) {
            int32_t w1 = gGameWorld->GetStaffHQ()->mTraining.mF00[1];
            int32_t w0 = gGameWorld->GetStaffHQ()->mTraining.mF00[0];
            int32_t w2 = gGameWorld->GetStaffHQ()->mTraining.mF00[2];
            int32_t w3 = gGameWorld->GetStaffHQ()->mTraining.mF00[3];
            int32_t w4 = gGameWorld->GetStaffHQ()->mTraining.mF00[4];
            int32_t total = w0 + (w1 + w2 + w3 + w4);
            summary->GetWindow(0x2df)->SetValue(total);
            summary->GetWindow(0x2e4)->SetValue(gStaffCostsDeposits - (gStaffCostsWord14 + total));
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

int32_t ShopStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ShopStatusPanel* panel = &gShopStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd1));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == 0xc06a) {
            base->GetNextObject();
        } else if (a > 0xc06a) {
            if (a == 0xc081) {
                UI_IterateThingList(kUIIterShops, _applytoall);
                UIStatusPanel_CallSlot(panel, 0x1c);
                return 0;
            }
            if (a > 0xc081) {
                if (a < 0xc083) {
                    base->CloseObject(b == 1 ? 1 : 0);
                    panel->UpdateStatusWindow();
                }
            } else if (a >= 0xc080) {
                base->Move();
                return 0;
            }
        } else {
            if (a == 0xc067) {
                CreateAllItemsPanel(kUIObjShop, gStaffRoomPanelThing);
                return 0;
            }
            if (a > 0xc067) {
                if (a >= 0xc069) {
                    base->GetPreviousObject();
                } else {
                    base->EraseObject();
                    return 0;
                }
            } else if (a < 0 && a >= -2) {
                panel->Remove();
                return 0;
            }
        }
    } else if (message == 0x800) {
        if (a == 0xc085) {
            panel->mF2c = b;
            UIStatusPanel_CallSlot(panel, 0x1c);
        } else if (a < 0xc085) {
            if (a == 0xc06b) {
                panel->mQuality = b;
                panel->mWindow->GetWindow(0xc07f)->SetValue((b & 0x20) != 0 ? 0 : static_cast<int32_t>(1u << (b & 0x1f)));   // slw: a shift of 32 to 63 gives 0
                UIStatusPanel_CallSlot(panel, 0x1c);
            }
        } else if (a < 0xc087) {
            panel->mIngredient = b;
            UIStatusPanel_CallSlot(panel, 0x1c);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}
