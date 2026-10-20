// Test library for the sideshows list rows and the all-items row window (unit v5335; libtpw_unit_UpdateSideshowListEntries.dylib,
// tests/test_UpdateSideshowListEntries.py), main app:
//   src/AddItemToAllRidesList_UpdateSideshowListEntries.cpp   UpdateSideshowListEntries, AllItemsListLCFormat (the Mac's LCFormat 0x10196388),
//                                                             gAllItemsRowHeight, gAllItemsFontIndex
//
// The callees outside the unit are recording fakes: the list's GetData and ChangeItem, the history's GetSumOfLast, the four CObject
// answers (read from words of the object the test builds), GetFontTableEntry, the font's virtual call, operator new, the window
// constructor, SetCallbackFunction and CreateObject. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "FillLenderList.h"
#include "History.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "InterfaceFont.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
int32_t gAllItemsListType;
int16_t gUIRealScreenHeight;

namespace {

uint32_t gItems[16];
uint64_t gLastNew;

uint32_t Word(const void* base, int32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const char*>(base) + offset, 4);
    return value;
}

constexpr ptrdiff_t kHistoryOffset = 0x1a4;

}  // namespace

int32_t AllItemsLCEntryHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {static_cast<int64_t>(position)});
    *data = static_cast<int32_t>(gItems[position & 15]);
    return 0;
}

void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {static_cast<int64_t>(position), static_cast<int64_t>(item[0].mInt), static_cast<int64_t>(item[1].mInt),
                      static_cast<int64_t>(item[2].mInt), static_cast<int64_t>(item[3].mInt), static_cast<int64_t>(item[4].mInt),
                      static_cast<int64_t>(item[5].mInt), static_cast<int64_t>(columns), static_cast<int64_t>(update),
                      static_cast<int64_t>(unused)});
}

template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    const char* object = reinterpret_cast<const char*>(this) - kHistoryOffset;
    ev("GetSumOfLast", {static_cast<int64_t>(Word(object, 0)), static_cast<int64_t>(count)});
    std::memcpy(&sum, object + 24, 8);
    return 0;
}

uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {static_cast<int64_t>(Word(this, 0)), static_cast<int64_t>(flag ? 1 : 0)});
    return static_cast<uint8_t>(Word(this, 4));
}

int32_t CObject::GetProfitability() const {
    ev("GetProfitability", {static_cast<int64_t>(Word(this, 0))});
    return static_cast<int32_t>(Word(this, 8));
}

int32_t CObject::GetTotalRevenue() const {
    ev("GetTotalRevenue", {static_cast<int64_t>(Word(this, 0))});
    return static_cast<int32_t>(Word(this, 12));
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {static_cast<int64_t>(Word(this, 0))});
    return static_cast<int32_t>(Word(this, 16));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x5000));
}

// Only the weak default of FontVirtual (replaced by the fake below) refers to it.
int32_t InterfaceFont::GetHeight() { return 0; }

namespace allitems_callee {
int16_t FontVirtual(InterfaceFont*) {
    ev("FontVirtual", {0});
    return static_cast<int16_t>(ret("FontVirtual"));
}
}  // namespace allitems_callee

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    void* block = ret("new", 1) ? tpw::ArenaAllocZeroed(size) : nullptr;
    gLastNew = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
    return block;
}

void WindowCtor(void*, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("WindowCtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(parent)), static_cast<int64_t>(attributes), static_cast<int64_t>(id),
                      static_cast<int64_t>(left), static_cast<int64_t>(top), static_cast<int64_t>(right), static_cast<int64_t>(bottom)});
}
}  // namespace lender_callee

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {callback == AllItemsLCEntryHandler ? 1 : 0});
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {static_cast<int64_t>(kind), static_cast<int64_t>(attributes), static_cast<int64_t>(id), static_cast<int64_t>(left),
                        static_cast<int64_t>(top), static_cast<int64_t>(right), static_cast<int64_t>(bottom)});
    return nullptr;
}

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rowheight", &gAllItemsRowHeight},
        {"fontindex", &gAllItemsFontIndex},
        {"listtype", &gAllItemsListType},
        {"screenheight", &gUIRealScreenHeight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_us_set_item(int32_t index, uint64_t address) { gItems[index & 15] = tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

uint64_t tpw_us_last_new() { return gLastNew; }

// The host's offset of the list's item count (the Mac's is 0x150).
int32_t tpw_us_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceListControl2, mItemCount));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    return n;
}

void tpw_us_update(uint64_t list) { UpdateSideshowListEntries(reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))); }

uint64_t tpw_us_lcformat() {
    gLastNew = 0;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(AllItemsListLCFormat()));
}

}  // extern "C"
