// Test library for the all-items list's attach function (unit v5560; libtpw_unit_LCPostFormat_6.dylib, tests/test_LCPostFormat_6.py), main app:
//   src/game/ui/ui_allItemsPanel.cpp   AllItemsListLCPostFormat
//
// Every callee outside the unit is a fake that records what the real one reads. The windows are blocks the test makes (never read by the
// code); GetChild and GetNext walk them in order. The two globals of MainWindowCallback.cpp are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
uint32_t gAllItemsFontIndex;
int32_t gAllItemsListType;

namespace {

constexpr int kWindows = 7;                 // 0 the row, 1..6 its columns in order
InterfaceWindow* gWin[kWindows];
InterfaceFont* gFont;

int IndexOf(const void* window) {
    for (int i = 0; i < kWindows; i++)
        if (gWin[i] == window) return i;
    return -1;
}

}  // namespace

// ---- the fakes
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return gFont;
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) {
    ev("UIMain_IndentClient", {lab(window), a, b});
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return gWin[1];
}

InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    int i = IndexOf(this);
    return i >= 1 && i + 1 < kWindows ? gWin[i + 1] : nullptr;
}

namespace allitems_post_callee {
void* OperatorNew(uint32_t size) {
    void* memory = tpw::ArenaAllocZeroed(size);
    ev("OperatorNew", {static_cast<int64_t>(size)});
    return memory;
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void GradientCtor(void* mem, const char* mesh) { ev("GradientCtor", {newlab(mem)}, {mesh}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace allitems_post_callee

extern "C" {

uint64_t tpw_lc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"type", &gAllItemsListType},
        {"font", &gAllItemsFontIndex},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The test's windows and its font object (made once, stable addresses): index 0 to 6 the windows, 7 the font.
uint64_t tpw_lc_block(int32_t index) {
    if (gWin[0] == nullptr) {
        for (int i = 0; i < kWindows; i++) gWin[i] = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(0x140));
        gFont = static_cast<InterfaceFont*>(tpw::ArenaAllocZeroed(0x40));
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(index == 7 ? static_cast<void*>(gFont) : static_cast<void*>(gWin[index])));
}

int32_t tpw_lc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 0;
    return n;
}

void tpw_lc_post(uint64_t window) { AllItemsListLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }

}  // extern "C"
