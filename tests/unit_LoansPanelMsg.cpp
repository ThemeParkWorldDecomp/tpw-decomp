// Test library for the loans panel (unit v5195; libtpw_unit_LoansPanelMsg.dylib, tests/test_LoansPanelMsg.py), main app:
//   src/ui_allKidsPanel_AllKidsPanelMsg_LoansPanelMsg.cpp   LoansPanelMsg, Payback, UpdateListHeadings (and gLoansPanel)
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The windows are real
// InterfaceWindow objects (the constructor and the virtuals are fakes, so that the class has a vtable here); the test makes six.
// gGameWorld, gLenderListMode and gText are the globals of other files, defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
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

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
int32_t gLenderListMode;
CText gText;

// gText's members are built by their constructors, which are in files this library does not compile: empty ones here.
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }   // the key function: never called, the fake FormatString does not format
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}

namespace {

InterfaceWindow* gWins[6];

int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

int64_t CallbackId(dlgcallback callback) {
    if (callback == nullptr) return 0;
    if (callback == PaybackLoanOK) return 1;
    if (callback == TakeLoanOK) return 2;
    return -1;
}

}  // namespace

// ---- the windows: the constructor and the virtuals (so the vtable is emitted here)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this), P(text)});
    return 1;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    char key[40];
    std::snprintf(key, sizeof key, "GetWindow:%d", id);
    int32_t k = static_cast<int32_t>(ret(key));
    return k != 0 ? gWins[k] : nullptr;
}

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), P(text)}); }

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("data"));
    return 0;
}

// ---- the world and the bank account
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x20000010));
}

CLoan CBankAccount::GetLoanInfo(uint32_t loan) {
    ev("GetLoanInfo", {lab(this), static_cast<int32_t>(loan)});
    CLoan result;
    result.mLoanAvailable = static_cast<int32_t>(ret("loan0"));
    result.mAmountAvailable = static_cast<uint32_t>(ret("loan1"));
    result.mAPRInPercent = static_cast<uint32_t>(ret("loan2"));
    result.mRepaymentPeriodInMonths = static_cast<uint32_t>(ret("loan3"));
    result.mMonthlyRepayment = static_cast<uint32_t>(ret("loan4"));
    result.mLoanBought = static_cast<int32_t>(ret("loan5"));
    result.mMonthsRepaid = static_cast<uint32_t>(ret("loan6"));
    result.mLenderNameIndex = static_cast<uint32_t>(ret("loan7"));
    return result;
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("balance"));
}

// ---- the string table
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list arguments;
    va_start(arguments, id);
    TbIntegerVTag* tag = va_arg(arguments, TbIntegerVTag*);
    va_end(arguments);
    ev("FormatString", {lab(this), static_cast<int32_t>(size), static_cast<int32_t>(id), static_cast<int32_t>(tag->mMagic),
                        static_cast<int32_t>(tag->mId), tag->mValue});
    out[0] = static_cast<char16_t>(id | 0x4000);
    out[1] = static_cast<char16_t>((static_cast<uint32_t>(tag->mValue) & 0x7fff) | 0x100);
    out[2] = 0;
    return 0;
}

// ---- the dialog
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("DialogBox", {static_cast<int32_t>(button1), CallbackId(callback1), static_cast<int32_t>(button2), CallbackId(callback2),
                     static_cast<int32_t>(button3), CallbackId(callback3), flag ? 1 : 0});
    int32_t length = 0;
    while (length < 256 && params.mText[length] != 0) ++length;
    ev("DialogParams", {params.mRed, params.mGreen, params.mBlue, params.mValue8, params.mValueC, length, params.mText[0], params.mText[1],
                        params.mText[2], params.mText[3]});
    return nullptr;
}

// ---- the other callees
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x30000000 + id * 16));
}

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("default"));
}

void RemoveLoansPanel() { ev("RemoveLoansPanel", {}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel", {}); }
int32_t TakeLoanOK(void*) { return 0; }
int32_t PaybackLoanOK(void*) { return 0; }

extern "C" {

uint64_t tpw_lm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"table", &gText.mResourcePointers[6]},
        {"panel", &gLoansPanel},
        {"mode", &gLenderListMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window object in a zeroed block (the fake constructor stores the vtable); the window number for GetWindow's answers.
void tpw_lm_makewin(int32_t number, uint64_t address) {
    gWins[number] = new (reinterpret_cast<void*>(static_cast<uintptr_t>(address))) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
}

// sizeof(InterfaceWindow), the host offsets of mHelpText (Mac 0x8c) and mF90 (Mac 0x90).
int32_t tpw_lm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    alignas(16) static unsigned char block[sizeof(InterfaceWindow) + 16];
    InterfaceWindow* window = reinterpret_cast<InterfaceWindow*>(block);
    out[n++] = reinterpret_cast<char*>(&window->mHelpText) - reinterpret_cast<char*>(window);
    out[n++] = reinterpret_cast<char*>(&window->mF90) - reinterpret_cast<char*>(window);
    return n;
}

uint32_t tpw_lm_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(LoansPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
void tpw_lm_payback(int32_t loan) { Payback(loan); }
void tpw_lm_update() { UpdateListHeadings(); }

}  // extern "C"
