// Loans panel (main app, ui_mainpanel2.cpp): LoansPanelMsg 0x101a3f6c (652 bytes), Payback 0x101a3cd4 (492), UpdateListHeadings 0x101a4230 (604).
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

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
