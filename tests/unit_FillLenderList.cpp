// Test library for the lenders list of the bank panel (unit v5145; libtpw_unit_FillLenderList.dylib, tests/test_FillLenderList.py), main app:
//   src/FillLenderList.cpp   FillLenderList, LenderLCFormat (LCFormat 0x101a44b8), LenderLCPostFormat (LCPostFormat 0x101a4660)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The objects of
// the case (the list, the world, the bank account, the string table, the font, the six windows) are static blocks that are only ever
// named by label: every callee that would read them is a fake.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int16_t gUIRealScreenHeight;
CWorld* gGameWorld;
// gText is raw bytes here: CText's constructor needs the vtables of the font wrappers, which this library does not link.
char gTextRaw[0x4000] __asm__("_gText");

namespace {
alignas(16) char gListBlock[0x400];
alignas(16) char gWorldBlock[0x400];
alignas(16) char gAccountBlock[0x400];
alignas(16) char gTableBlock[0x400];
alignas(16) char gFontBlock[0x400];
alignas(16) char gWindows[6][0x400];
unsigned char gStrings[16];
char16_t gTokens[16];

int32_t Ret(const char* key, int64_t arg) { return static_cast<int32_t>(ret(key, arg, 0)); }
}  // namespace

// ---- the callees outside the unit: one fake each, the same event name and arguments as the module's PowerPC stub
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gAccountBlock);
}

CLoan CBankAccount::GetLoanInfo(uint32_t loan) {
    ev("GetLoanInfo", {lab(this), static_cast<int64_t>(loan)});
    CLoan result;
    const int32_t base = static_cast<int32_t>(loan) * 8;
    result.mLoanAvailable = Ret("loan", base + 0);
    result.mAmountAvailable = static_cast<uint32_t>(Ret("loan", base + 1));
    result.mAPRInPercent = static_cast<uint32_t>(Ret("loan", base + 2));
    result.mRepaymentPeriodInMonths = static_cast<uint32_t>(Ret("loan", base + 3));
    result.mMonthlyRepayment = static_cast<uint32_t>(Ret("loan", base + 4));
    result.mLoanBought = Ret("loan", base + 5);
    result.mMonthsRepaid = static_cast<uint32_t>(Ret("loan", base + 6));
    result.mLenderNameIndex = static_cast<uint32_t>(Ret("loan", base + 7));
    return result;
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), static_cast<int64_t>(id)});
    return &gStrings[Ret("cs", id) & 15];
}

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    const int32_t k = static_cast<int32_t>(text - gStrings);
    ev("ConvertMulti", {k});
    return &gTokens[Ret("cm", k) & 15];
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), static_cast<int64_t>(item[0].mText - gTokens), item[1].mInt, item[2].mInt, item[3].mInt, item[4].mInt,
                   item[5].mInt, data, position, update});
    return 0;
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFontBlock);
}

// The Mac calls the font's virtual at slot 0xc of the table at +0x10 (the module's __ptr_glue stub tells it from the window's by r12).
int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {lab(this)});
    return Ret("height", 0);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == LendersItemsCallback ? 1 : 0});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

InterfaceWindow* InterfaceWindow::GetChild(int32_t) {
    ev("GetChild", {lab(this)});
    return reinterpret_cast<InterfaceWindow*>(gWindows[1]);
}

InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    for (int i = 0; i < 5; ++i)
        if (reinterpret_cast<char*>(this) == gWindows[i]) return reinterpret_cast<InterfaceWindow*>(gWindows[i + 1]);
    return nullptr;
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t a, int32_t b, int32_t kind, int16_t c, int16_t d, int16_t e, int16_t f) {
    ev("CreateObject", {lab(this), a, b, kind, c, d, e, f});
    return nullptr;
}

int32_t LendersItemsCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size < 8 ? 8 : size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem), 0}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem), 0}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem), 0}); }
void PercentCtor(void* mem) { ev("PercentCtor", {newlab(mem), 0}); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {newlab(mem), static_cast<int64_t>(reinterpret_cast<uintptr_t>(parent)), attributes, id, left, top, right, bottom});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee

// The weak program definitions of lender_callee in the unit's source call the real constructors; these labels satisfy the linker (the
// test library's own lender_callee functions replace those definitions, so the labels are never reached).
extern "C" {
void tpw_never_window() __asm__("__ZN15InterfaceWindowC1EPS_iissss");
void tpw_never_sprite() __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void tpw_never_cash() __asm__("__ZN23InterfaceCashTextObjectC1EPv");
void tpw_never_number() __asm__("__ZN25InterfaceNumberTextObjectC1EPv");
void tpw_never_percent() __asm__("__ZN26InterfacePercentTextObjectC1EPv");
void tpw_never_window() {}
void tpw_never_sprite() {}
void tpw_never_cash() {}
void tpw_never_number() {}
void tpw_never_percent() {}
}

extern "C" {

// The host address of a global or an object by the module's name for it.
uint64_t tpw_lender_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"mode", &gLenderListMode},  {"rowh", &gLenderRowHeight}, {"fontidx", &gLenderFontIndex}, {"screenh", &gUIRealScreenHeight},
        {"world", &gGameWorld},      {"text", &gTextRaw},         {"list", gListBlock},           {"worldobj", gWorldBlock},
        {"acct", gAccountBlock},     {"table", gTableBlock},      {"font", gFontBlock},           {"win0", gWindows[0]},
        {"win1", gWindows[1]},       {"win2", gWindows[2]},       {"win3", gWindows[3]},          {"win4", gWindows[4]},
        {"win5", gWindows[5]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lender_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The string table the list reads its names from: gText's pointer 18 (Mac 0x594).
void tpw_lender_set_table(uint64_t table) {
    gText.mResourcePointers[18] = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
}

void tpw_lender_set_world(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }

int32_t tpw_lender_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CLoan));
    out[n++] = static_cast<int64_t>(offsetof(CLoan, mLenderNameIndex));
    out[n++] = static_cast<int64_t>(sizeof(LCWord));
    return n;
}

void tpw_lender_fill(uint64_t list) { FillLenderList(reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))); }
void tpw_lender_post(uint64_t window) { LenderLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }
int32_t tpw_lender_format() { return static_cast<int32_t>(lab(LenderLCFormat())); }

}  // extern "C"
