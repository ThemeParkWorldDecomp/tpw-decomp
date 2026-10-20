// Test library for HideIfUnder and AerialCallback (unit v5979; libtpw_unit_HideIfUnder.dylib, tests/test_HideIfUnder.py), main app:
//   src/ui_mainpanel2_msg_HideIfUnder.cpp   HideIfUnder, AerialCallback, gAerialSlide
//
// The InterfaceWindow members and CoolRect pieces the two functions call are recording fakes (no window is ever built: the test
// allocates zeroed blocks and the fakes only use the address as a label). The window tree and every answer come from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
InterfaceWindow* gTagFirstWindow;

namespace {

InterfaceWindow* gWins[16];

InterfaceWindow* Win(int64_t label) { return (label > 0 && label < 16) ? gWins[label] : nullptr; }

int64_t Q(const char* base, void* window) {
    char key[64];
    int64_t l = lab(window);
    std::snprintf(key, sizeof key, "%s%d", base, static_cast<int>(l));
    return ret(key);
}

int16_t S16(int64_t v) { return static_cast<int16_t>(v); }

}  // namespace

// ---- CoolRegion / CoolRect: only the constructor and destructor are logged; the rest exists for the vtables
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- InterfaceWindow members
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t flag) {
    ev("GetPosition", {lab(this), flag});
    rect->mLeft = S16(Q("pos_l", this));
    rect->mTop = S16(Q("pos_t", this));
    rect->mRight = S16(Q("pos_r", this));
    rect->mBottom = S16(Q("pos_b", this));
    return 1;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return Win(Q("child", this));
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    return Win(Q("next", this));
}
uint32_t InterfaceWindow::GetAttr() {
    ev("GetAttr", {lab(this)});
    return static_cast<uint32_t>(Q("attr", this));
}
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    char key[32];
    std::snprintf(key, sizeof key, "extra%d", index);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret(key))));
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)))});
    return 1;
}
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) {
    ev("link", {lab(this), lab(child), a, b});
}
int32_t InterfaceWindow::GetDims(int16_t* width, int16_t* height, int16_t adjust) {
    ev("GetDims", {lab(this), adjust});
    *width = S16(ret("dimw"));
    *height = S16(ret("dimh"));
    return 1;
}
void InterfaceWindow::AdjustPosition(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("AdjustPosition", {lab(this), left, top, right, bottom});
}
int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    ev("ShiftPosition", {lab(this), dx, dy});
    return 1;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Win(ret("getwindow"));
}
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), lab(anim), ticks});
    return 1;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    // a window travelling as the argument is logged as its label (its word is an arena offset here, an address on the Mac)
    int64_t la = a != 0 ? lab(tpw::gArenaBase + static_cast<uint32_t>(a)) : 0;
    ev("CallDefaultWindowHandler", {lab(this), message, la == -1 ? static_cast<int64_t>(a) : la, b});
    return static_cast<int32_t>(ret("default"));
}

// ---- free functions
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("send"));
}
int32_t UI_PutDeferredMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t delay) {
    ev("UI_PutDeferredMessage", {lab(window), message, a, b, delay});
    return 0;
}
namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("Enable", {lab(window), on}); }
}  // namespace entry_callee

extern "C" {

uint64_t tpw_hiu_addr(const char* name) {
    if (std::strcmp(name, "first") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gTagFirstWindow));
    if (std::strcmp(name, "delta") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gAerialSlide));
    return 0;
}

uint64_t tpw_hiu_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_hiu_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

void tpw_hiu_setwin(int32_t label, uint64_t address) { gWins[label] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(address)); }

int32_t tpw_hiu_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mTop));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mRight));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mBottom));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mAnimating));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

void tpw_hiu_hide(uint64_t window, int16_t limit) {
    HideIfUnder(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), limit);
}

int32_t tpw_hiu_aerial(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return AerialCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
