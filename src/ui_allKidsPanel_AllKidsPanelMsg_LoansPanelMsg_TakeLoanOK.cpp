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

// 0x101a3ee0: the OK button of the take-loan dialog; the parameter is the loan number. Takes the loan, refreshes the lender list
// (window id 0x12c4bd of the loans panel, no NULL tests) and answers 1.
int32_t TakeLoanOK(void* parameter) {
    gGameWorld->GetBankAccount()->ApplyForLoan(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter)));
    FillLenderList(static_cast<InterfaceListControl2*>(gLoansPanel->GetWindow(0x12c4bd)));
    return 1;
}
