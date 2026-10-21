// Test library for the main panel's slide-out slot (unit v5262; libtpw_unit_LinkSlideoutPanel.dylib, tests/test_LinkSlideoutPanel.py), main app:
//   src/game/ui/ui_mainpanel2.cpp   LinkSlideoutPanel, RemoveSlideoutPanel
//
// The callees outside the unit (GetWindow, getExtraParam, setExtraParam, UI_SendMessage, UI_PostMessage, SetState) are recording fakes.
// gMainPanel and gGameWindow are defined here. Six zeroed blocks (labels 1..5; 0 is NULL) stand for the windows.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gMainPanel;
InterfaceWindow* gGameWindow;

namespace {
void* gBlk[6];

void* Blk(int i) {
    if (gBlk[1] == nullptr)
        for (int k = 1; k < 6; ++k) gBlk[k] = tpw::ArenaAllocZeroed(0x800);
    return gBlk[i];
}
}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    const int64_t k = ret("GetWindow", id, 0);
    return k > 0 && k < 6 ? static_cast<InterfaceWindow*>(Blk(static_cast<int>(k))) : nullptr;
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("getExtraParam")));
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int64_t>(reinterpret_cast<uintptr_t>(value))});
    return 0;
}

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    // a is a pointer truncated to 32 bits (the blocks are 32-bit addressable): logged as the block's label
    // (the host's arena may sit above 4 GB: the low 32 bits are matched against the blocks)
    int64_t label = a == 0 ? 0 : -1;
    for (int k = 1; k < 6; ++k)
        if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Blk(k))) == static_cast<uint32_t>(a)) label = k;
    ev("UI_PostMessage", {lab(window), message, label, b});
    return 0;
}

extern "C" {

uint64_t tpw_lsp_addr(const char* name) {
    if (std::strcmp(name, "gMainPanel") == 0) return reinterpret_cast<uintptr_t>(&gMainPanel);
    if (std::strcmp(name, "gGameWindow") == 0) return reinterpret_cast<uintptr_t>(&gGameWindow);
    if (std::strncmp(name, "blk", 3) == 0) return reinterpret_cast<uintptr_t>(Blk(name[3] - '0'));
    return 0;
}

uint64_t tpw_lsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mF134));
    return n;
}

uint64_t tpw_lsp_link(uint64_t panel, int32_t n) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LinkSlideoutPanelResult(reinterpret_cast<InterfaceWindow*>(panel), n)));
}

void tpw_lsp_remove(int32_t which) { RemoveSlideoutPanel(static_cast<int16_t>(which)); }

}  // extern "C"
