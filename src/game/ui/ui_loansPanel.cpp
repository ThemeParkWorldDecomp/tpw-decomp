// ui_loansPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LLCellSystem.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// 0x101a3c44 (104 bytes): pays the loan off (the answer is not used), then lists the lenders again. Answers 1.
int32_t PaybackLoanOK(void* parameter) {
    gGameWorld->GetBankAccount()->PayOffLoan(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter)));
    FillLenderList(static_cast<InterfaceListControl2*>(gLoansPanel->GetWindow(0x12c4bd)));
    return 1;
}

// Loans panel (main app, ui_mainpanel2.cpp): LoansPanelMsg 0x101a3f6c (652 bytes), Payback 0x101a3cd4 (492), UpdateListHeadings 0x101a4230 (604).

InterfaceWindow* gLoansPanel;   // 0x10394980

namespace {

constexpr int32_t kTextTable = 6;   // gText.mResourcePointers[6] (Mac 0x564)

// Formats string `id` with one integer tag (tag id 1) into `text` and shows it in a dialog; `parameter` is the dialog's value at +0x0c.
void ShowLoanDialog(char16_t* text, uint32_t id, int32_t value, int32_t parameter, dlgbutton button1, dlgcallback callback1,
                    dlgbutton button2, dlgcallback callback2) {
    TbIntegerVTag tag(1, value);
    static_cast<TbITable*>(gText.mResourcePointers[kTextTable])->FormatString(text, 0xff, id, &tag);
    dlgboxparams params;
    params.mText = text;
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = parameter;
    UI_DialogBox(params, button1, callback1, button2, callback2, static_cast<dlgbutton>(0), nullptr, false);
}

// One heading: the window, then the string, then the text for the client draw object and the two help text words.
void SetHeading(InterfaceWindow* parent, int32_t windowId, int32_t stringId, int32_t help) {
    InterfaceWindow* window = parent->GetWindow(windowId);
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
    window->mHelpText = help;
    window->mF90 = help;
}

}  // namespace

// 0x101a3cd4
void Payback(int32_t loan) {
    CLoan info = gGameWorld->GetBankAccount()->GetLoanInfo(static_cast<uint32_t>(loan));
    int32_t cost = static_cast<int32_t>(info.mMonthlyRepayment * (info.mRepaymentPeriodInMonths - info.mMonthsRepaid));
    char16_t text[256];
    if (gGameWorld->GetBankAccount()->GetBalance() > cost) {
        ShowLoanDialog(text, 0xb5, cost, loan, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), PaybackLoanOK);
    } else {
        ShowLoanDialog(text, 0xb6, cost, loan, static_cast<dlgbutton>(1), nullptr, static_cast<dlgbutton>(0), nullptr);
    }
}

// 0x101a3ee0: the OK button of the take-loan dialog; the parameter is the loan number. Takes the loan, refreshes the lender list
// (window id 0x12c4bd of the loans panel, no NULL tests) and answers 1.
int32_t TakeLoanOK(void* parameter) {
    gGameWorld->GetBankAccount()->ApplyForLoan(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter)));
    FillLenderList(static_cast<InterfaceListControl2*>(gLoansPanel->GetWindow(0x12c4bd)));
    return 1;
}

// 0x101a3f6c
int32_t LoansPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x1e) {
        if (message < 0x1e) {
            if (message == 0x14) {
                gLoansPanel = nullptr;
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc3));
            } else if (message < 0x14 && message == 5) {
                RemoveLoansPanel();
                return 1;
            }
        } else if (message == 0x400) {
            InterfaceWindow* list = window->GetWindow(a);
            if (list != nullptr && b != -1) {
                int32_t data;
                static_cast<InterfaceListControl2*>(list)->GetData(b, &data);
                if (data < 0) {
                    Payback(-(data + 1));
                } else {
                    int32_t loan = data - 1;
                    CLoan info = gGameWorld->GetBankAccount()->GetLoanInfo(static_cast<uint32_t>(loan));
                    char16_t text[64];
                    ShowLoanDialog(text, 0xb7, static_cast<int32_t>(info.mAmountAvailable), loan, static_cast<dlgbutton>(2), nullptr,
                                   static_cast<dlgbutton>(1), TakeLoanOK);
                }
            }
        } else if (message < 0x400 && message == 0x100) {
            if (a == 0x12c4ba) {
                CreateEntryPricePanel();
                return 0;
            }
            if (a < 0x12c4ba) {
                if (a < 0 && a > -3) {
                    RemoveLoansPanel();
                    return 0;
                }
            } else {
                if (a == 0x12c4bc) {
                    CreateStaffCostsPanel();
                    return 0;
                }
                if (a < 0x12c4bc) {
                    CreateFinancePanel();
                    return 0;
                }
            }
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x101a4230
void UpdateListHeadings() {
    if (gLoansPanel == nullptr) return;
    InterfaceWindow* parent = gLoansPanel->GetWindow(0x12c4bd);
    if (gLenderListMode == 0) {
        SetHeading(parent, 0x11, 0xae, 0xdf);
        SetHeading(parent, 0x12, 0xad, 0xe0);
        SetHeading(parent, 0x15, 0xb1, 0xe3);
        const char16_t* title = UIMain_GetString(0xaa);
        gLoansPanel->GetWindow(0x12c4ca)->SetText(const_cast<char16_t*>(title));
    } else {
        SetHeading(parent, 0x11, 0xb2, 0xe5);
        SetHeading(parent, 0x12, 0xb3, 0xe6);
        SetHeading(parent, 0x15, 0xb4, 0xe7);
        const char16_t* title = UIMain_GetString(0xab);
        gLoansPanel->GetWindow(0x12c4ca)->SetText(const_cast<char16_t*>(title));
    }
}

// Unit v5145: the lenders list of the bank panel. FillLenderList 0x101a4a38, LCPostFormat 0x101a4660 (here LenderLCPostFormat) and
// LCFormat 0x101a44b8 (here LenderLCFormat): the names exist at other addresses.

// The program's definitions of lender_callee (weak: the test library of this unit defines its own, recording ones).

static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceWindow) <= 0x400, "the allocation below is 0x400 at least");

namespace lender_callee {
// The Mac allocates the sizes of its objects; the host's are larger, so the block is at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
__attribute__((weak)) void NumberCtor(void* mem) { new (mem) InterfaceNumberTextObject(nullptr); }
__attribute__((weak)) void PercentCtor(void* mem) { new (mem) InterfacePercentTextObject(nullptr); }
__attribute__((weak)) void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                      int16_t right, int16_t bottom) {
    new (mem) InterfaceWindow(parent, attributes, id, left, top, right, bottom);
}
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace lender_callee

int32_t gLenderListMode;                  // 0x1039497c
int32_t gLenderRowHeight;                  // 0x10394978
uint32_t gLenderFontIndex = 7;             // 0x102aa838

// 0x101a4a38: empties the list, then adds the lenders of the bank account's eight loans. Mode 0 lists the loans on offer (available and
// not bought), mode 1 the loans taken. An item is: the lender's name, the amount, the period, the APR, the monthly repayment and the
// total (repayment times period). The loans taken show the months still to pay, and carry the negated number as their data.
void FillLenderList(InterfaceListControl2* list) {
    list->RemoveAllItems();
    for (uint32_t i = 0; i < 8; ++i) {
        CLoan loan = gGameWorld->GetBankAccount()->GetLoanInfo(i);
        UI_LCAdd item[6] = {};
        if (gLenderListMode == 0 && loan.mLoanBought == 0 && loan.mLoanAvailable != 0) {
            const unsigned char* name = reinterpret_cast<TbITable*>(gText.mResourcePointers[18])->GetConstString(loan.mLenderNameIndex);
            item[0].mText = UIMain_ConvertMulti(name);
            item[1].mInt = static_cast<int32_t>(loan.mAmountAvailable);
            item[2].mInt = static_cast<int32_t>(loan.mRepaymentPeriodInMonths);
            item[3].mInt = static_cast<int32_t>(loan.mAPRInPercent);
            item[4].mInt = static_cast<int32_t>(loan.mMonthlyRepayment);
            item[5].mInt = static_cast<int32_t>(loan.mMonthlyRepayment * loan.mRepaymentPeriodInMonths);
            list->AddItem(item, static_cast<int32_t>(i + 1), -1, 1);
        } else if (gLenderListMode == 1 && loan.mLoanBought != 0) {
            const unsigned char* name = reinterpret_cast<TbITable*>(gText.mResourcePointers[18])->GetConstString(loan.mLenderNameIndex);
            item[0].mText = UIMain_ConvertMulti(name);
            item[1].mInt = static_cast<int32_t>(loan.mAmountAvailable);
            const int32_t left = static_cast<int32_t>(loan.mRepaymentPeriodInMonths - loan.mMonthsRepaid);
            item[2].mInt = left;
            item[3].mInt = static_cast<int32_t>(loan.mAPRInPercent);
            item[4].mInt = static_cast<int32_t>(loan.mMonthlyRepayment);
            item[5].mInt = static_cast<int32_t>(loan.mMonthlyRepayment) * left;
            list->AddItem(item, -static_cast<int32_t>(i + 1), -1, 1);
        }
    }
}

// 0x101a44b8: the row window of the list (five controls of kinds 2 to 6). The row height is the font's height scaled to the UI's 1536
// lines, plus 6, worked out on the first call. (The Mac calls the font's virtual at slot 0xc of the table at +0x10: GetHeight.)
InterfaceWindow* LenderLCFormat() {
    if (gLenderRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gLenderFontIndex);
        const int16_t height = static_cast<int16_t>(font->GetHeight());
        gLenderRowHeight = static_cast<int32_t>(height) * 0x600 / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gLenderRowHeight));
    InterfaceWindow* row = static_cast<InterfaceWindow*>(memory);
    row->CreateObject(1, 1, 2, 0, 0, 0, 0);
    row->CreateObject(1, 1, 3, 0, 0, 0, 0);
    row->CreateObject(1, 1, 4, 0, 0, 0, 0);
    row->CreateObject(1, 1, 5, 0, 0, 0, 0);
    row->CreateObject(1, 1, 6, 0, 0, 0, 0);
    return row;
}

// 0x101a4660: gives the row and its five children their draw objects, font, handler and justification (each child's SetValue(0) is the
// window's virtual at slot 0x24).
void LenderLCPostFormat(InterfaceWindow* window) {
    using namespace lender_callee;
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    window->SetFont(GetFontTableEntry(gLenderFontIndex));
    window->SetCallbackFunction(LendersItemsCallback);
    InterfaceWindow* child = window->GetChild(0);
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) NumberCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) PercentCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
}

// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).

// 0x101a4bec (184 bytes)
int32_t LenderListCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        gLenderListMode = (a == 0x12c4bf) ? 1 : 0;
        FillLenderList(reinterpret_cast<InterfaceListControl2*>(window));
        UpdateListHeadings();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a4ce4 (192 bytes)
int32_t LendersItemsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        if (b < 0)
            window->SetFontColour(0x8c, 0, 0x3c, 0xff);
        else
            window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// CreateLoansPanel 0x101a4de4 (2320 bytes): builds the loans panel (unit v5698). Neighbour of FillLenderList.cpp.

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

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x101a571c
void RemoveLoansPanel() { UI_SendMessage(gLoansPanel, 4, 0, 0); }

// Unit v5995 (main app, ui_mainpanel2.cpp): the main panel's date display, the last-panel memory, the loans panel refresh and the
// slide-out panel's message function.
//   UI_MainPanelDateChange 0x101a6234 (416 bytes), SetLastPanel 0x101a57fc (72), UI_LoansPanelDateChange 0x101a577c (80),
//   RailsCallback 0x101a6f2c (1932).

// 0x101a577c (80 bytes): refills the lender list of the loans panel while it shows the loans taken (mode 1). The month is unused.
void UI_LoansPanelDateChange(int32_t month) {
    (void)month;
    if (gLoansPanel != nullptr && gLenderListMode == 1) {
        InterfaceWindow* list = gLoansPanel->GetWindow(0x12c4bd);
        FillLenderList(static_cast<InterfaceListControl2*>(list));
    }
}
