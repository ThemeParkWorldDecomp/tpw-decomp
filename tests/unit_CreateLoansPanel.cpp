// Test library for CreateLoansPanel (unit v5698; libtpw_unit_CreateLoansPanel.dylib, tests/test_CreateLoansPanel.py), main app:
//   src/game/ui/ui_loansPanel.cpp   CreateLoansPanel and the globals gLoansScript, gLenderColumnColour, the game type storage and guard
//
// The callees outside the unit are fakes that record what the real ones read (windows are slots of a block the test gives; pointers go
// through lab()) and answer from the test's queues. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "FS_Online.h"
#include "FillLenderList.h"
#include "History.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
InterfaceWindow* gLoansPanel;
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;
int32_t gLenderListMode;

namespace {
uint8_t* gWinBase;   // 64 slots of 0x200 bytes
uint8_t* gStrBase;   // the strings: id * 4

void* Slot(int k) { return gWinBase + k * 0x200; }
int SlotOfId(int32_t id) {
    if (id >= 0x12c4bd && id <= 0x12c4ca) return id - 0x12c4bd + 1;
    if (id >= 0x10 && id <= 0x15) return 20 + id;
    return 60;
}
int64_t Off(const void* p) { return p == nullptr ? -1 : static_cast<int64_t>(static_cast<const uint8_t*>(p) - gStrBase); }
int64_t Code(const void* p) { return p == nullptr ? 0 : 1; }
const int kRoot = 0, kAnalyser = 58, kBank = 57, kWorld = 56, kDrawObj = 61, kFont = 62, kGameWindow = 63;
}  // namespace

// ---- the fakes
CGameType::CGameType() {
    ev("CGameType", {static_cast<int64_t>(static_cast<void*>(this) == static_cast<void*>(gLoansGameTypeStorage))});
    mType = static_cast<GameType>(static_cast<int32_t>(ret("gtype")));
}

bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(gStrBase + id * 4);
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {lab(window), id, Off(text)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(Slot(kFont));
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int64_t>(message)}); }
void UpdateListHeadings() { ev("UpdateListHeadings", {}); }
void FillLenderList(InterfaceListControl2* list) { ev("FillLenderList", {lab(list)}); }
int32_t LoansPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t LenderListCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void LenderLCPostFormat(InterfaceWindow*) {}
InterfaceWindow* LenderLCFormat() { return nullptr; }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), static_cast<int64_t>(description == gLoansScript) + 2 * static_cast<int64_t>(callback == LoansPanelMsg)});
    return ret("build") != 0 ? reinterpret_cast<InterfaceWindow*>(Slot(kRoot)) : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0x12c4c1 && ret("summary") == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(Slot(SlotOfId(id)));
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), static_cast<int64_t>(callback == LenderListCallback)});
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), Off(text)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), static_cast<int64_t>(makeRow == LenderLCFormat), static_cast<int64_t>(attach == LenderLCPostFormat) * 2});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {lab(this), column, static_cast<int64_t>(type)}); }
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 1;
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {name[0], name[1], name[2], name[3], name[4], name[5], name[6], name[7]});
    return reinterpret_cast<InterfaceDrawObject*>(Slot(kDrawObj));
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MakeColumnButton", {lab(window), Off(text), a, b, c, d, e});
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(Slot(kAnalyser));
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(Slot(kBank));
}
CLoan CBankAccount::GetLoanInfo(uint32_t loan) {
    ev("GetLoanInfo", {lab(this), static_cast<int64_t>(loan)});
    CLoan result;
    result.mLoanAvailable = static_cast<int32_t>(ret("loan"));
    result.mAmountAvailable = static_cast<uint32_t>(ret("loan"));
    result.mAPRInPercent = static_cast<uint32_t>(ret("loan"));
    result.mRepaymentPeriodInMonths = static_cast<uint32_t>(ret("loan"));
    result.mMonthlyRepayment = static_cast<uint32_t>(ret("loan"));
    result.mLoanBought = static_cast<int32_t>(ret("loan"));
    result.mMonthsRepaid = static_cast<uint32_t>(ret("loan"));
    result.mLenderNameIndex = static_cast<uint32_t>(ret("loan"));
    return result;
}

template <>
bool CHistory<int32_t, 144>::GetValue(int32_t& out, int32_t back) const {
    ev("GetValue", {static_cast<int64_t>(reinterpret_cast<const uint8_t*>(this) - static_cast<uint8_t*>(Slot(kAnalyser))), back});
    out = static_cast<int32_t>(ret("hist"));
    return true;
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void NumberCtor(void*) {}
void PercentCtor(void*) {}
void WindowCtor(void*, InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee

extern "C" {

uint64_t tpw_ccl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gLoansPanel},       {"gametype", gLoansGameTypeStorage}, {"guard", &gLoansGameTypeGuard},
        {"world", &gGameWorld},        {"window", &gGameWindow},            {"mode", &gLenderListMode},
        {"colour", &gLenderColumnColour},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ccl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ccl_blocks(uint64_t windows, uint64_t strings) {
    gWinBase = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(windows));
    gStrBase = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(strings));
}

int32_t tpw_ccl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100));
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CLoan, mMonthlyRepayment));
    out[n++] = static_cast<int64_t>(offsetof(CLoan, mLoanBought));
    out[n++] = static_cast<int64_t>(sizeof(CLoan));
    out[n++] = static_cast<int64_t>(sizeof(CGameType));
    return n;
}

void tpw_ccl_CreateLoansPanel() { CreateLoansPanel(); }

}  // extern "C"
