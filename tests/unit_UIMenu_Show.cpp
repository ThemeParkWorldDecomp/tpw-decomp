// Test library for UIMenu::Show, Hide and ~UIMenu (unit v5771; libtpw_unit_UIMenu_Show.dylib, tests/test_UIMenu_Show.py), main app:
//   src/ui_main_kbcapture_UIMenu_AddItem_UIMenu_Show.cpp
//
// The InterfaceWindow methods the menu calls, UI_GetHighestZ, UIMain_CheckPanel, UIMain_DecideKbCapture, UI_SendMessage and the runtime's
// operator delete are recording fakes; gUIRootWindow is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ui_menu.h"

using namespace calllog;

InterfaceWindow* gUIRootWindow;

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
void InterfaceWindow::unlink() { ev("unlink", {lab(this)}); }
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) { ev("link", {lab(this), lab(child), a, b}); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
void InterfaceWindow::UnCaptureKb(int32_t on) { ev("UnCaptureKb", {lab(this), on}); }

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect) {
    ev("UI_GetHighestZ", {lab(window), lab(rect)});
    return static_cast<int32_t>(ret("Z"));
}
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Msg"));
}
void menu_callee::OperatorDelete(void* p) { ev("delete", {lab(p)}); }

extern "C" {

uint64_t tpw_menu_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"root", &gUIRootWindow},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_menu_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_menu_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mShown));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mWindow));
    out[n++] = static_cast<int64_t>(sizeof(UIMenu));
    return n;
}

void tpw_menu_show(uint64_t menu, int32_t a) { reinterpret_cast<UIMenu*>(menu)->Show(a); }
void tpw_menu_hide(uint64_t menu) { reinterpret_cast<UIMenu*>(menu)->Hide(); }
uint64_t tpw_menu_dtor(uint64_t menu, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(UIMenuDestruct(reinterpret_cast<UIMenu*>(menu), static_cast<int16_t>(flag))));
}

}  // extern "C"
