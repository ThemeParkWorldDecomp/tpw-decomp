// Test library for ShowStatusPanel::UpdateWindow, __sinit_ui_upgradeStatus_cpp and YearEndPanelMsg (unit v51007;
// libtpw_unit_ShowStatusPanel_UpdateWindow_2.dylib, tests/test_ShowStatusPanel_UpdateWindow_2.py), main app:
//   src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp
//
// The callees outside the unit are recording fakes; the panel's table entries and the windows' virtuals are exported function
// pointers (tpw_us_fn) that the test puts in the table blocks. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "MainWindowCallback.h"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing;
InterfaceWindow* gYearEndPanel;
alignas(16) uint8_t gUpgradePanelStorage[0x40] __asm__("_gUpgradeStatusPanel");

namespace {

uint64_t gLabelAddr[64];

void* AddrOf(int64_t label) { return label > 0 && label < 64 ? reinterpret_cast<void*>(static_cast<uintptr_t>(gLabelAddr[label])) : nullptr; }

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0xa08a && ret("Win8aNull")) return nullptr;
    int64_t label = 0;
    switch (id) {
        case 0xa08a: label = 6; break;
        case 0xa08e: label = 7; break;
        case 0xa08f: label = 8; break;
        case 0xa090: label = 9; break;
        case 0xa0a6: label = 10; break;
        case 0xa0a7: label = 11; break;
        case 0xa0a5: label = 12; break;
        default: break;
    }
    return static_cast<InterfaceWindow*>(AddrOf(label));
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}

bool CObject::IsClosed() const {
    ev("IsClosed", {lab(this)});
    return ret("IsClosed") != 0;
}

uint8_t CObject::GetChanceOfWinning() const {
    ev("GetChanceOfWinning", {lab(this)});
    return static_cast<uint8_t>(ret("GetChanceOfWinning"));
}

uint32_t CObject::GetPricePerUse() const {
    ev("GetPricePerUse", {lab(this)});
    return static_cast<uint32_t>(ret("GetPricePerUse"));
}

uint32_t CObject::GetPrizeValue() const {
    ev("GetPrizeValue", {lab(this)});
    return static_cast<uint32_t>(ret("GetPrizeValue"));
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return static_cast<RideInfo*>(AddrOf(3));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return static_cast<const char16_t*>(AddrOf(id == 0x33 ? 14 : id == 0x34 ? 15 : -1));
}

void UIStatusPanel_Construct(UIStatusPanel* self) { ev("Construct", {lab(self)}); }

void RemoveYearEndPanel() { ev("RemoveYearEndPanel", {}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultPanelMsg"));
}

namespace parkstatus_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("WindowSetText", {lab(window), lab(text)}); }
}  // namespace parkstatus_callee

namespace {
void Slot1c(void* self) { ev("Slot1c", {lab(self)}); }
void Slot20(void* self) { ev("Slot20", {lab(self)}); }
const char16_t* Slot3c(void* self) {
    ev("Slot3c", {lab(self)});
    return static_cast<const char16_t*>(AddrOf(16));
}
}  // namespace

// The table the static initialiser stores (src/ui_StatusPanel_tables.cpp, unit v5L31): a block of the arena made on first use; the test
// compares only where it is stored.
uint32_t* UpgradeStatusPanelTable() {
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(0x80));
    return table;
}

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"upgrade", gUpgradePanelStorage},
        {"yearend", &gYearEndPanel},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_us_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

void tpw_us_setthing(uint64_t address) { gStaffRoomPanelThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(address)); }

void tpw_us_setlabel(int32_t label, uint64_t address) { gLabelAddr[label] = address; }

// The table entries: kind 0x1c, 0x20, 0x3c.
uint64_t tpw_us_fn(int32_t kind) {
    if (kind == 0x1c) return reinterpret_cast<uintptr_t>(&Slot1c);
    if (kind == 0x20) return reinterpret_cast<uintptr_t>(&Slot20);
    return reinterpret_cast<uintptr_t>(&Slot3c);
}

void tpw_us_dbset(uint64_t address, int32_t value) {
    reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(address))->Value(kRideSlot_NonWinnableSideshow).mInteger = value;
}

int32_t tpw_us_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mMode));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(UIStatusPanelFields));
    return n;
}

void tpw_us_updatewindow(uint64_t panel) { reinterpret_cast<ShowStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateWindow(); }

void tpw_us_sinit() { __sinit_ui_upgradeStatus_cpp(); }
uint32_t tpw_us_sinit_table() { return tpw::ArenaOffset(UpgradeStatusPanelTable()); }

int32_t tpw_us_yearend(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return YearEndPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
