// Test library for UIMenu_DefaultItemCallback and UIMenu_DefaultCallback (unit v5575), main app:
//   src/InterfaceWindow_virtuals_UIMenu_DefaultItemCallback.cpp
// The callees are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"

using namespace calllog;

namespace {
void* gExtra0 = nullptr;
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {index});
    if (index == 0) return gExtra0;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("extra1"))));
}

void InterfaceStringHeap::ReleaseString(char16_t* text) {
    ev("ReleaseString", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
}

void UIMain_UISound(int32_t code) { ev("UISound", {code}); }

int32_t InterfaceWindow::GetID() {
    ev("GetID", {});
    return static_cast<int32_t>(ret("id"));
}

InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("parent"))));
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(window)), message, a, b});
    return static_cast<int32_t>(ret("send"));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {message, a, b});
    return static_cast<int32_t>(ret("default"));
}

extern "C" {

uint64_t tpw_uimenu_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_uimenu_set_extra(uint64_t p) { gExtra0 = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

// The host menu's layout: out[0] the offset of mWindow, out[1] its size, out[2] sizeof(UIMenu).
int32_t tpw_uimenu_layout(int64_t* out) {
    out[0] = static_cast<int64_t>(offsetof(UIMenu, mWindow));
    out[1] = static_cast<int64_t>(sizeof(InterfaceWindow*));
    out[2] = static_cast<int64_t>(sizeof(UIMenu));
    return 3;
}

uint32_t tpw_uimenu_item(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(UIMenu_DefaultItemCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}

uint32_t tpw_uimenu_def(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(UIMenu_DefaultCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}

}  // extern "C"
