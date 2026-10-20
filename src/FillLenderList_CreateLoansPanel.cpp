// CreateLoansPanel 0x101a4de4 (2320 bytes): builds the loans panel (unit v5698). Neighbour of FillLenderList.cpp.
#include "FillLenderList.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"
#include <algorithm>
#include <new>

#include "FS_Online.h"
#include "History.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ThingParts2.h"
#include "analyser.h"
#include "WindowScripts.inc"

int16_t gLoansScript[] = {TPW_WINDOW_SCRIPT_102AA4A4};   // 0x102aa4a4: the image's words (generated, devtools/gen_game_tables.py)
int32_t gLenderColumnColour = 8;      // 0x102aa83c
alignas(4) unsigned char gLoansGameTypeStorage[4];   // 0x102b3aa0
int8_t gLoansGameTypeGuard;                           // 0x102b3aa4

namespace {

// A text sprite object (0x19c bytes) as the loans panel makes it.
InterfaceDrawObject* NewSprite() {
    void* mem = lender_callee::OperatorNew(0x19c);
    if (mem != nullptr) lender_callee::SpriteCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

InterfaceDrawObject* NewCash() {
    void* mem = lender_callee::OperatorNew(0x19c);
    if (mem != nullptr) lender_callee::CashCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

// A label of the summary: the text object, the font 6, optionally red, the text.
void Label(InterfaceWindow* window, int32_t text, bool red) {
    window->SetClientDrawObject(NewSprite());
    if (red) window->SetFontColour(0xff, 0, 0, 0xff);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(text)));
}

// The first part of a cash figure: the cash object, right/centre justification and the font 6.
void CashSetup(InterfaceWindow* window) {
    window->SetClientDrawObject(NewCash());
    window->SetFontJust(2, 1);
    window->SetFont(GetFontTableEntry(6));
}

}  // namespace

void CreateLoansPanel() {
    if (gLoansPanel != nullptr) return;
    if (gLoansGameTypeGuard == 0) {
        new (gLoansGameTypeStorage) CGameType();
        gLoansGameTypeGuard = 1;
    }
    if (reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType == kGameTypeEasy) return;
    UIMain_CheckPanel();
    SetLastPanel(3, 8);
    InterfaceWindow* built = gGameWindow->Build(gLoansScript, LoansPanelMsg);
    if (built == nullptr) return;
    gLoansPanel = built;
    UIMain_SetPanel(gLoansPanel);
    UIMain_SetTitleText(gLoansPanel, 0x12c4ca, const_cast<char16_t*>(UIMain_GetString(0xaa)));
    InterfaceWindow* summary = gLoansPanel->GetWindow(0x12c4c1);
    if (summary != nullptr) {
        CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
        Label(summary->GetWindow(0x12c4c4), 0x165, false);
        Label(summary->GetWindow(0x12c4c9), 0x168, true);
        Label(summary->GetWindow(0x12c4c6), 0x167, true);
        Label(summary->GetWindow(0x12c4c2), 0x169, false);
        int32_t deposits = 0;
        int32_t withdrawals = 0;
        analyser->mHistory1F100[5].GetValue(deposits, 0);
        InterfaceWindow* window = summary->GetWindow(0x12c4c5);
        CashSetup(window);
        lender_callee::WindowSetValue(window, deposits);
        window = summary->GetWindow(0x12c4c8);
        int32_t repayments = 0;
        for (uint32_t i = 0; i < 8; ++i) {
            CLoan loan = gGameWorld->GetBankAccount()->GetLoanInfo(i);
            if (loan.mLoanBought != 0) repayments += static_cast<int32_t>(loan.mMonthlyRepayment);
        }
        CashSetup(window);
        lender_callee::WindowSetValue(window, repayments);
        window->SetFontColour(0xff, 0, 0, 0xff);
        analyser->mHistory1F100[2].GetValue(withdrawals, 0);
        window = summary->GetWindow(0x12c4c7);
        CashSetup(window);
        lender_callee::WindowSetValue(window, withdrawals - repayments);
        window->SetFontColour(0xff, 0, 0, 0xff);
        window = summary->GetWindow(0x12c4c3);
        CashSetup(window);
        lender_callee::WindowSetValue(window, deposits - withdrawals);
    }
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gLoansPanel->GetWindow(0x12c4bd));
    list->SetColumnFormatsAll(LenderLCFormat, LenderLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    list->SetColumnType(2, kLCNumber);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetColumnType(5, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    list->SetCallbackFunction(LenderListCallback);
    FillLenderList(list);
    InterfaceWindow* button = list->GetWindow(0x10);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0xac)), gLenderColumnColour, 0xff, 0xef, 0, 0xde);
    button = list->GetWindow(0x13);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0xaf)), gLenderColumnColour, 0xff, 0xef, 0, 0xe1);
    button = list->GetWindow(0x14);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0xb0)), gLenderColumnColour, 0xff, 0xef, 0, 0xe2);
    MAKECOLUMNBUTTON(list->GetWindow(0x11), nullptr, gLenderColumnColour, 0xff, 0xef, 0, -1);
    MAKECOLUMNBUTTON(list->GetWindow(0x12), nullptr, gLenderColumnColour, 0xff, 0xef, 0, -1);
    MAKECOLUMNBUTTON(list->GetWindow(0x15), nullptr, gLenderColumnColour, 0xff, 0xef, 0, -1);
    InterfaceCheckButtons* modes = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x12c4be));
    if (gLenderListMode == 0) {
        modes->SetCurrent(0x12c4c0);
    } else {
        modes->SetCurrent(0x12c4bf);
    }
    UpdateListHeadings();
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc3));
}
