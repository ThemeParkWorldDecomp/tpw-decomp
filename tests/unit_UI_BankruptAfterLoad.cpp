// Test library for UI_BankruptAfterLoad (libtpw_unit_UI_BankruptAfterLoad.dylib, tests/test_UI_BankruptAfterLoad.py).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_Postcard.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gBankruptWindow;
InterfaceWindow* gBumholeWindow;
int16_t gBankruptScript[4];
UI_RectDelta gBankruptAnim[2];
static InterfaceWindow* gFakeBuilt;

int32_t BankruptWindowMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

int32_t GameWindowToggle_Hidden() {
    ev("Hidden", {});
    return static_cast<int32_t>(ret("Hidden"));
}
void ToggleGameWindow() { ev("ToggleGameWindow", {}); }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gBankruptScript ? 1 : 0, reinterpret_cast<void*>(callback) == reinterpret_cast<void*>(BankruptWindowMsg) ? 1 : 0});
    return gFakeBuilt;
}
CoolRegion::~CoolRegion() {}   // base destructor, not part of the unit (never logged: the PowerPC side fakes CoolRect's only)
// The vtables of CoolRect and CoolRegion land here (their destructors are defined here); these are never called.
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mRight = static_cast<int16_t>(ret("right"));
    rect->mBottom = static_cast<int16_t>(ret("bottom"));
    return 0;
}
void InterfaceWindow::setPivot(int16_t x, int16_t y) { ev("setPivot", {lab(this), x, y}); }
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), anim == gBankruptAnim ? 1 : 0, ticks});
    return 0;
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

extern "C" {

uint64_t tpw_ba_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"bankrupt", &gBankruptWindow},
        {"bumhole", &gBumholeWindow},
        {"built", &gFakeBuilt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ba_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ba_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_ba_run() { UI_BankruptAfterLoad(); }

}  // extern "C"
