// ui_StaffCostsPanel.cpp: name proven by __sinit_ui_StaffCostsPanel_cpp in the main application.

#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "History.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>

// UpdateStaffCost 0x101be030 (412 bytes), StaffCostsPanelMsg 0x101be1f4 (524 bytes), ShopStatusMsg 0x101bdb8c (796 bytes).

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

// CreateStaffCostsPanel 0x101be440 (3596 bytes, ui_StaffCostsPanel.cpp): builds the staff costs panel (the five staff wage boxes with their quality
// bars, and the finance summary of window 0x2de) unless it is up already.

InterfaceWindow* gStaffCostsPanel = nullptr;   // 0x10395a20
int32_t gStaffCostsDeposits = 0;               // 0x10395a1c
int32_t gStaffCostsWithdrawals = 0;            // 0x10395a18
int32_t gStaffCostsWord10 = 0;                 // 0x10395a10
int32_t gStaffCostsWord14 = 0;                 // 0x10395a14
int16_t gStaffCostsScript[] = {TPW_WINDOW_SCRIPT_102ADC20};   // 0x102adc20: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// One staff wage row: the box (window `id`) with its cash text, its range and increment, its font, the wage of the staff type and, in its child 0x2e8,
// the gradient bar of the quality percentage.
void StaffCostRow(int32_t id, int32_t index, int32_t model, CParkAnalyser* analyser) {
    int32_t minimum = 0;
    int32_t maximum = 10000;
    int32_t increment = 0x19;
    InterfaceWindow* window = gStaffCostsPanel->GetWindow(id);
    InterfaceCashTextObject* cash = new InterfaceCashTextObject(nullptr);
    window->SetClientDrawObject(cash);
    static_cast<InterfaceVaryBox*>(window)->SetMinMaxValues(&minimum, &maximum);
    minimum = 0;
    static_cast<InterfaceVaryBox*>(window)->SetInc(&increment);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontJust(1, 1);
    window->SetFontColour(0, 0, 0, 0xff);
    CStaffHQ* headquarters = gGameWorld->GetStaffHQ();
    window->SetValue(static_cast<int32_t>(headquarters->mTraining.mF00[index]));
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("ridestatbar.wct"));
    window->GetWindow(0x2e8)->SetClientDrawObject(gradient);
    int32_t quality = analyser->AverageQualityOfStaffType(static_cast<CThing::ThingModel>(model));
    int32_t percent = static_cast<int32_t>(static_cast<uint32_t>(quality) << 10) / 100;
    window->GetWindow(0x2e8)->SetValue(percent);
}

// A text sprite in window `id` of `parent`: the font and the string; `red` makes the text red.
void TextRow(InterfaceWindow* parent, int32_t id, bool red, int32_t string) {
    InterfaceWindow* window = parent->GetWindow(id);
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    if (red) window->SetFontColour(0xff, 0, 0, 0xff);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
}

// A cash text in window `id` of `parent`, right aligned, in the font; the value comes from the caller after the window is made.
InterfaceWindow* CashRow(InterfaceWindow* parent, int32_t id) {
    InterfaceWindow* window = parent->GetWindow(id);
    InterfaceCashTextObject* cash = new InterfaceCashTextObject(nullptr);
    window->SetClientDrawObject(cash);
    window->SetFontJust(2, 1);
    window->SetFont(GetFontTableEntry(6));
    return window;
}

}  // namespace

// 0x101be440 (3596 bytes)
void CreateStaffCostsPanel() {
    if (gStaffCostsPanel != nullptr) return;
    UIMain_CheckPanel();
    SetLastPanel(3, 9);
    InterfaceWindow* built = gGameWindow->Build(gStaffCostsScript, StaffCostsPanelMsg);
    if (built == nullptr) return;
    gStaffCostsPanel = built;
    UIMain_SetPanel(gStaffCostsPanel);
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    StaffCostRow(0x2e7, 0, 5, analyser);
    StaffCostRow(0x2ec, 1, 4, analyser);
    StaffCostRow(0x2ea, 2, 6, analyser);
    StaffCostRow(0x2eb, 3, 7, analyser);
    StaffCostRow(0x2ed, 4, 8, analyser);
    InterfaceWindow* summary = gStaffCostsPanel->GetWindow(0x2de);
    if (summary != nullptr) {
        CParkAnalyser* park = gGameWorld->GetParkAnalyser();
        TextRow(summary, 0x2e3, false, 0x165);
        TextRow(summary, 0x2e6, true, 0x166);
        TextRow(summary, 0x2e1, true, 0x167);
        TextRow(summary, 0x2e5, false, 0x169);
        gStaffCostsDeposits = 0;
        gStaffCostsWord10 = 0;
        gStaffCostsWithdrawals = 0;
        park->mHistory1F100[5].GetValue(gStaffCostsDeposits, 0);
        InterfaceWindow* window = CashRow(summary, 0x2e2);
        window->SetValue(gStaffCostsDeposits);
        park->mHistory1F100[3].GetValue(gStaffCostsWord10, 0);
        window = CashRow(summary, 0x2df);
        window->SetValue(gStaffCostsWord10);
        window->SetFontColour(0xff, 0, 0, 0xff);
        park->mHistory1F100[2].GetValue(gStaffCostsWithdrawals, 0);
        gStaffCostsWord14 = gStaffCostsWithdrawals - gStaffCostsWord10;
        window = CashRow(summary, 0x2e0);
        window->SetValue(gStaffCostsWord14);
        window->SetFontColour(0xff, 0, 0, 0xff);
        window = CashRow(summary, 0x2e4);
        window->SetValue(gStaffCostsDeposits - gStaffCostsWithdrawals);
    }
    UIMain_SetTitleText(gStaffCostsPanel, 0x2da, const_cast<char16_t*>(UIMain_GetString(0x93)));
    if (TheGameType().mType == kGameTypeEasy) gStaffCostsPanel->GetWindow(0x2dd)->Enable(0);
}

// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.

// Unit v51121: the two words the static initialiser of ui_StaffCostsPanel.cpp sets (names invented).
int32_t gStaffCostsF24;   // 0x10395a24
int32_t gStaffCostsF28;   // 0x10395a28

void RemoveStaffCostsPanel() { UI_SendMessage(gStaffCostsPanel, 4, 0, 0); }   // 0x101bf278
void __sinit_ui_StaffCostsPanel_cpp() {                                       // 0x101bf2dc
    gStaffCostsF28 = 0;
    gStaffCostsF24 = 0x10000;
}
