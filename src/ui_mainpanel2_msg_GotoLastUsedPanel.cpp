// ui_mainpanel2.cpp (main app), unit v5188: GotoLastUsedPanel 0x101a587c, DoCashWin 0x101a5ca8.
#include <cstdint>
#include <new>

#include "analyser.h"
#include "ThingParts2.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "ThingReceivers.h"
#include "Mouse.h"
#include "LLCellSystem.h"
#include "FillLenderList.h"

int32_t gCashColourIndex;                                    // 0x103949a0
uint8_t gCashInitDone;                                       // 0x103949a4
int32_t gCashSessionCost;                                    // 0x103949a8
int32_t gCashFontCurrent = 999;                              // 0x102aad28
// 0x102aad2c: red, orange, amber, yellow, pale yellow and white, from a balance of 0 or less up to 4001 and more (read from the image).
uint8_t gCashFontColours[24] = {0xff, 0x00, 0x00, 0xff, 0xfa, 0x5a, 0x00, 0xff, 0xff, 0xaf, 0x00, 0xff,
                                0xff, 0xc8, 0x00, 0xff, 0xff, 0xff, 0x96, 0xff, 0xff, 0xff, 0xff, 0xff};

// 0x101a587c (380 bytes): opens the panel last used of the toolbar's group (1 lists, 2 park, 3 finance), unless a menu is up.
void GotoLastUsedPanel(int32_t type) {
    if (MainMenuUp() != 0) return;
    switch (type) {
        case 1:
            if (gLastPanelLists == 2) {
                CreateHireListPanel(static_cast<StaffType>(-1));
            } else if (gLastPanelLists == 1) {
                tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected;
                selected.mIndex = 0;   // the original stores 0 into the half of the stack word that is the argument
                CreateRideListPanel(static_cast<UIObjType>(-1), selected);
            }
            break;
        case 2:
            switch (gLastPanelPark) {
                case 3: CreateParkStatusPanel(); break;
                case 4: CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr); break;
                case 5: CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); break;
                case 6: CreateAllKidsPanel(nullptr); break;
                default: break;
            }
            break;
        case 3:
            switch (gLastPanelFinance) {
                case 7: CreateFinancePanel(); break;
                case 8: CreateLoansPanel(); break;
                case 9: CreateStaffCostsPanel(); break;
                case 10: CreateEntryPricePanel(); break;
                default: break;
            }
            break;
        default: break;
    }
}

// 0x101a5ca8 (996 bytes): keeps the cash window up to date: the balance's colour and value, and the session cost of the cell modes.
void DoCashWin(InterfaceWindow* window) {
    if (static_cast<int8_t>(gCashInitDone) == 0) {
        gCashSessionCost = -999;
        gCashInitDone = 1;
    }
    int32_t balance = gGameWorldRef->GetBankAccount()->GetBalance();
    if (balance != gCashWindowWord) {
        gCashColourIndex = (balance + 999) / 1000;
        if (gCashColourIndex < 0) gCashColourIndex = 0;
        if (gCashColourIndex > 5) gCashColourIndex = 5;
        if (gCashColourIndex != gCashFontCurrent) {
            const uint8_t* colour = gCashFontColours + gCashColourIndex * 4;
            window->SetFontColour(colour[0], colour[1], colour[2], 255);
            gCashFontCurrent = gCashColourIndex;
        }
        lender_callee::WindowSetValue(window, balance < 0 ? -balance : balance);   // abs, then the virtual SetValue (slot 0x24)
        CoolRect rect54;
        CoolRect rect48;
        CoolRect rect3c;
        window->GetSizeOfClientDrawObject(&rect54);
        InterfaceWindow* w31 = window->GetWindow(0x31);
        w31->SetPosition(&rect54.mRight, nullptr);
        InterfaceWindow* w30 = window->GetWindow(0x30);
        window->GetPosition(&rect48, 1);
        w30->GetPosition(&rect3c, 1);
        int16_t x = static_cast<int16_t>(rect54.mRight - (rect3c.mRight - rect3c.mLeft));
        w30->SetPosition(&x, nullptr);
        if (balance < 0) window->GetWindow(0x32)->SetFrame(1);
        else window->GetWindow(0x32)->SetFrame(0);
        gCashWindowWord = balance;
    }
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(lender_callee::OperatorNew(1));
        if (mouse != nullptr) {
            void* storage = lender_callee::OperatorNew(8);
            CMMDefault* mode = nullptr;
            if (storage != nullptr) mode = new (storage) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    int32_t mouseMode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (static_cast<uint32_t>(mouseMode - 3) <= 1 || (mouseMode == 8 && IsCoasterLoftMode() != 0)) {
        int32_t cost = LLCC_SystemGetSessionCost();
        if (cost != gCashSessionCost) {
            InterfaceWindow* w = window->GetWindow(0x30);
            if (w != nullptr) {
                if (cost != -999) w->Show(1);
                if (cost == -9999) {
                    w->Show(0);
                } else {
                    lender_callee::WindowSetValue(w, cost);
                    gCashSessionCost = cost;
                }
            }
        }
    } else if (gCashSessionCost != -999) {
        InterfaceWindow* w = window->GetWindow(0x30);
        if (w != nullptr) w->Show(0);
        gCashSessionCost = -999;
    }
}
