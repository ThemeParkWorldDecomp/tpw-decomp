// Test library for the all-rides and all-shops panels (unit v5964; libtpw_unit_CreateAllRidesPanel.dylib, tests/test_CreateAllRidesPanel.py):
//   src/game/ui/ui_allItemsPanel.cpp
// Every callee outside the unit is a fake that records what the PowerPC stub records.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gAllItemsPanel = nullptr;
InterfaceListControl2* gAllItemsList = nullptr;
int32_t gAllItemsListType = 0;
int32_t gAllRidesSort = 1;
int32_t gAllShopsSort = 1;
int32_t gAllColumnButtonFont = 8;

namespace {
void* gObj[7];

int64_t Fn(const void* p) {
    if (p == reinterpret_cast<const void*>(&ActualListCallback)) return 1;
    if (p == reinterpret_cast<const void*>(&AllItemsListLCFormat)) return 2;
    if (p == reinterpret_cast<const void*>(&AllItemsListLCPostFormat)) return 3;
    if (p == reinterpret_cast<const void*>(&FillAllRidesListCallback)) return 4;
    if (p == reinterpret_cast<const void*>(&FillAllRidesListCallback_Upgrades)) return 5;
    if (p == reinterpret_cast<const void*>(&FillAllShopsListCallback)) return 6;
    return 0;
}
}  // namespace

// ---- callbacks used only as addresses
int32_t ActualListCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindow* AllItemsListLCFormat() { return nullptr; }
void AllItemsListLCPostFormat(InterfaceWindow*) {}
int32_t FillAllRidesListCallback(CThing*) { return 0; }
int32_t FillAllRidesListCallback_Upgrades(CThing*) { return 0; }
int32_t FillAllShopsListCallback(CThing*) { return 0; }

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    int64_t l = lab(this);
    ev("GetWindow", {l, id});
    if (l == 1 && id == 0x12c4b8) return static_cast<InterfaceWindow*>(gObj[1]);
    if (l == 2 && id == 400) return ret("old") ? static_cast<InterfaceWindow*>(gObj[2]) : nullptr;
    if (l == 4 && id >= 0x10 && id <= 0x14) return static_cast<InterfaceWindow*>(gObj[4]);
    return nullptr;
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return 0;
}

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gAllRidesScript ? 1 : 0, static_cast<int64_t>(reinterpret_cast<uintptr_t>(callback))});
    return static_cast<InterfaceWindow*>(gObj[3]);
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), Fn(reinterpret_cast<const void*>(callback))});
}

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), Fn(reinterpret_cast<const void*>(makeRow)), Fn(reinterpret_cast<const void*>(attach))});
}

void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {std::strcmp(name, "hilight") == 0 ? 1 : 0});
    return static_cast<InterfaceDrawObject*>(gObj[6]);
}

void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }

void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("IterateThingList", {static_cast<int32_t>(mask), Fn(reinterpret_cast<const void*>(fn))});
    return 0;
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

int32_t InterfaceListControl2::FindData(int32_t data) {
    int64_t t = data == static_cast<int32_t>(tpw::ArenaOffset(gObj[5])) ? lab(gObj[5]) : -1;
    ev("FindData", {lab(this), t});
    return static_cast<int32_t>(ret("find"));
}

void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), position}); }

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(gObj[6 - 0]);
}

void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MakeColumnButton", {lab(window), lab(text), a, b, c, d, e});
}

extern "C" {

uint64_t tpw_cap_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gAllItemsPanel}, {"list", &gAllItemsList}, {"type", &gAllItemsListType},
        {"ridesort", &gAllRidesSort}, {"shopsort", &gAllShopsSort}, {"font", &gAllColumnButtonFont},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_cap_objects(uint64_t* out) {
    for (int i = 0; i < 7; i++) {
        gObj[i] = tpw::ArenaAllocZeroed(0x200);
        out[i] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObj[i]));
    }
}

void tpw_cap_rides(uint64_t thing) { CreateAllRidesPanel(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }
void tpw_cap_shops(uint64_t thing) { CreateAllShopsPanel(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }

}  // extern "C"
