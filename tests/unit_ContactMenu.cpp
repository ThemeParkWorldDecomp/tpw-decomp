// Test library for the online panel's contact helpers (unit v5660; libtpw_unit_ContactMenu.dylib, tests/test_ContactMenu.py), main app:
//   src/OnlineParkFindPanelMsg_ContactMenu.cpp   FillBuddyList, ButtonsMenuMsg, ContactMenu
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue; the globals of
// other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "online.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindowE8c;
InterfaceWindow* gOnlineWindowE90;
int32_t gOnlineWord88;
COnlineControl* gOnlineControl;

static uint64_t gTabPtr;

namespace {

uint64_t* Tab() { return reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(gTabPtr)); }
void* TabEntry(int64_t idx) { return reinterpret_cast<void*>(static_cast<uintptr_t>(Tab()[idx])); }

void NameEvent(const char* name, COnlineControl* control, std::u16string& text) {
    const char16_t* c = text.c_str();
    ev(name, {lab(control), c[0], c[1], c[2], c[3]});
}

}  // namespace

// ---- the CoolRegion / CoolRect members the rectangle on the stack needs (the base class's are not compiled in this library)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::CoolRect() {
    ev("CoolRect::CoolRect", {lab(this)});
    mLeft = mTop = mRight = mBottom = 0;
}
CoolRect::~CoolRect() { ev("CoolRect::~CoolRect", {lab(this)}); }

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(TabEntry(ret("GetWindow")));
}
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 1;
}
void InterfaceListControl2::GetItem(int32_t position, int32_t column, UI_LCAdd& out) {
    ev("GetItem", {lab(this), position, column});
    out.mRaw = 0;
    out.mText = static_cast<const char16_t*>(TabEntry(ret("GetItem")));
}
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }
COnlineControl::COnlineControl() { ev("COnlineControl::COnlineControl", {newlab(this)}); }
bool COnlineControl::IsThisMe(const char16_t* name) {
    ev("IsThisMe", {lab(this), lab(name)});
    return ret("IsThisMe") != 0;
}
void COnlineControl::UpdateBuddyList() const { ev("UpdateBuddyList", {lab(this)}); }
void COnlineControl::BlackMark(std::u16string& n) { NameEvent("BlackMark", this, n); }
void COnlineControl::IgnoreOn(std::u16string& n) { NameEvent("IgnoreOn", this, n); }
void COnlineControl::IgnoreOff(std::u16string& n) { NameEvent("IgnoreOff", this, n); }
void COnlineControl::Goto(std::u16string& n) { NameEvent("Goto", this, n); }
void COnlineControl::BuddyOn(std::u16string& n) { NameEvent("BuddyOn", this, n); }
void COnlineControl::BuddyOff(std::u16string& n) { NameEvent("BuddyOff", this, n); }
void COnlineControl::Locate(std::u16string& n) { NameEvent("Locate", this, n); }
void COnlineControl::Page(std::u16string& n) { NameEvent("Page", this, n); }
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) {
    ev("link", {lab(this), lab(child), static_cast<uint8_t>(a), static_cast<uint8_t>(b)});
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    int64_t v = ret("GetPosition");
    rect->mLeft = static_cast<int16_t>(v >> 16);
    rect->mTop = static_cast<int16_t>(v);
    return 0;
}
void UI_GetMouseXY(int16_t& x, int16_t& y) {
    ev("UI_GetMouseXY", {});
    int64_t v = ret("Mouse");
    x = static_cast<int16_t>(v >> 16);
    y = static_cast<int16_t>(v);
}
int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    ev("ShiftPosition", {lab(this), dx, dy});
    return 0;
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 0;
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 0;
}
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return TabEntry(ret("ExtraParam"));
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}
void HideButtonsMenu() { ev("HideButtonsMenu", {}); }

extern "C" {

uint64_t tpw_cm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlinePanel", &gOnlinePanel}, {"gE90", &gOnlineWindowE90}, {"gE8c", &gOnlineWindowE8c},
        {"gW88", &gOnlineWord88},        {"gControl", &gOnlineControl}, {"tabptr", &gTabPtr},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mTop));
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    return n;
}

void tpw_cm_fill() { FillBuddyList(); }
uint32_t tpw_cm_buttons(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(ButtonsMenuMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
void tpw_cm_contact(int32_t position) { ContactMenu(position); }

}  // extern "C"
