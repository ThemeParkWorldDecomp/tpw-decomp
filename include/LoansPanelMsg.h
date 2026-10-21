// Unit v5195: the loans panel's message function, the payback dialog and the lender list headings
// (main app, src/game/ui/ui_loansPanel.cpp).
#pragma once
#include <cstdint>

class InterfaceWindow;

// 0x10394980 (BSS): the loans panel window; NULL while there is none (FillLenderList's panel). LoansPanelMsg clears it on message 0x14.
extern InterfaceWindow* gLoansPanel;

// 0x101a3f6c (652 bytes): the loans panel's message function. Message 5 removes the panel (1); 0x14 clears gLoansPanel and cancels the
// advisor message 0xc3; 0x100 handles the panel's buttons (a: -2 and -1 remove it, 0x12c4bb finance, 0x12c4ba entry prices, 0x12c4bc staff
// costs; each answers 0); 0x400 is a click in a loan list (a the list's id, b the row): a taken loan (negative data) asks to pay it back,
// an offered loan asks to take it. Anything else, and the cases that do not answer, go to UIMain_DefaultPanelMsg.
int32_t LoansPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x101a3cd4 (492 bytes): asks whether to pay the loan back (two buttons, PaybackLoanOK) when the balance is above the remaining
// instalments (monthly repayment times the months left), else says it cannot be afforded (one button).
void Payback(int32_t loan);
// 0x101a4230 (604 bytes): the three column headings and the title of the panel's list for the offered (mode 0) or the taken loans.
// No NULL tests on the windows.
void UpdateListHeadings();

// Stand-ins and unit-less callees (main app): 0x101a571c; 0x101a3ee0 (the dialog's OK button for taking a loan); 0x101a3c44 (for paying back).
void RemoveLoansPanel();
int32_t TakeLoanOK(void* parameter);
int32_t PaybackLoanOK(void* parameter);
