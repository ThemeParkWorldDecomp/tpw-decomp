// Test library for the main panel's last-used-panel jump and the cash window (unit v5188; libtpw_unit_GotoLastUsedPanel.dylib,
// tests/test_GotoLastUsedPanel.py), main app:
//   src/game/ui/ui_mainpanel2.cpp   GotoLastUsedPanel, DoCashWin; defines the cash window's own globals
//
// The panels, the window members, the bank, the mouse and the cell system are recording fakes that answer from the test's queues. The globals
// of other files the unit reads or writes are defined here. The global operator new is replaced so that the two news of DoCashWin are logged.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ThingParts2.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "ThingReceivers.h"
#include "Mouse.h"
#include "LLCellSystem.h"
#include "FillLenderList.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gCashWindowWord;     // 0x103949ac (src/game/ui/ui_mainpanel2.cpp)
int32_t gLastPanelFinance;   // 0x103949b8
int32_t gLastPanelPark;      // 0x103949bc
int32_t gLastPanelLists;     // 0x103949c0
int32_t gMouseWord1E0;       // 0x102dc1e0
int32_t gMouseWord1E4;       // 0x102dc1e4
CMouseMode* gMouseMode;      // 0x102dc1f0
CMouse* gMouse;              // 0x102dc1f4
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;

namespace {

// The mouse mode the window asks: GetMode is a fake.
struct FakeMode : CMouseMode {
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};
FakeMode gFakeMode;
InterfaceWindow* gWindows[3];   // the children 0x30, 0x31, 0x32

}  // namespace

// ---- the replaced operator new: logged while a case is running
namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size ? size : 4);
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee

// ---- the mouse
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// ---- the world and the bank
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x5000));
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("balance"));
}

// ---- the cell system and the mode test
int32_t IsCoasterLoftMode() {
    ev("IsCoasterLoftMode", {});
    return static_cast<int32_t>(ret("IsCoasterLoftMode"));
}
int32_t LLCC_SystemGetSessionCost() {
    ev("LLCC_SystemGetSessionCost", {});
    return static_cast<int32_t>(ret("cost"));
}

// ---- the window's members
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
static void FillRect(CoolRect* rect, const char* key) {
    uint32_t a = static_cast<uint32_t>(ret(key)), b = static_cast<uint32_t>(ret(key));
    rect->mLeft = static_cast<int16_t>(a >> 16);
    rect->mTop = static_cast<int16_t>(a);
    rect->mRight = static_cast<int16_t>(b >> 16);
    rect->mBottom = static_cast<int16_t>(b);
}
void InterfaceWindow::GetSizeOfClientDrawObject(CoolRect* rect) {
    ev("GetSizeOfClientDrawObject", {lab(this)});
    FillRect(rect, "GetSize");
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int64_t present = id == 0x30 ? ret("GW30") : id == 0x31 ? ret("GW31") : id == 0x32 ? ret("GW32") : 0;
    return present ? gWindows[id - 0x30] : nullptr;
}
int32_t InterfaceWindow::SetPosition(int16_t* x, int16_t* y) {
    ev("SetPosition", {lab(this), x ? *x : 0x10000, y ? *y : 0x10000});
    return 1;
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    FillRect(rect, "GetPosition");
    return 1;
}
void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}

// ---- the panels
int32_t MainMenuUp() {
    ev("MainMenuUp", {});
    return static_cast<int32_t>(ret("MainMenuUp"));
}
void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected) {
    ev("CreateRideListPanel", {static_cast<int32_t>(type), selected.mIndex});
}
void CreateHireListPanel(StaffType type) { ev("CreateHireListPanel", {static_cast<int32_t>(type)}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {static_cast<int32_t>(type), lab(thing)}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), lab(thing)}); }
void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {lab(thing)}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CreateLoansPanel() { ev("CreateLoansPanel", {}); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel", {}); }

extern "C" {

// The host address of a 32-bit global by the module's name for it (ADDR's keys).
uint64_t tpw_gcw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"cashword", &gCashWindowWord},     {"colouridx", &gCashColourIndex}, {"initdone", &gCashInitDone},
        {"sessioncost", &gCashSessionCost}, {"fontcur", &gCashFontCurrent},   {"colours", gCashFontColours},
        {"mouse1e0", &gMouseWord1E0},       {"mouse1e4", &gMouseWord1E4},     {"lastfin", &gLastPanelFinance},
        {"lastpark", &gLastPanelPark},      {"lastlists", &gLastPanelLists},  {"fakemode", &gFakeMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The pointer globals (64 bits here): which 0 gMouse, 1 gMouseMode, 2 gGameWorld.
void tpw_gcw_setptr(int32_t which, uint64_t value) {
    if (which == 0) gMouse = reinterpret_cast<CMouse*>(static_cast<uintptr_t>(value));
    if (which == 1) gMouseMode = reinterpret_cast<CMouseMode*>(static_cast<uintptr_t>(value));
    if (which == 2) gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(value));
}
uint64_t tpw_gcw_getptr(int32_t which) {
    if (which == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMouse));
    if (which == 1) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMouseMode));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gGameWorld));
}

// A zeroed block (32-bit addressable).
uint64_t tpw_gcw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gcw_layout(int64_t*) { return 0; }

// The window handed to GetWindow: ids 0x30, 0x31, 0x32 answer these blocks (set by the test before the call).
void tpw_gcw_setwindows(uint64_t w30, uint64_t w31, uint64_t w32) {
    gWindows[0] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w30));
    gWindows[1] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w31));
    gWindows[2] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w32));
}

void tpw_gcw_goto(int32_t type) { GotoLastUsedPanel(type); }
void tpw_gcw_cash(uint64_t window) { DoCashWin(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }

}  // extern "C"
