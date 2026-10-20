// CreateStaffCostsPanel 0x101be440 (3596 bytes, ui_StaffCostsPanel.cpp): builds the staff costs panel (the five staff wage boxes with their quality
// bars, and the finance summary of window 0x2de) unless it is up already.
#include <cstdint>

#include "FS_Online.h"
#include "History.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "WindowScripts.inc"

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
