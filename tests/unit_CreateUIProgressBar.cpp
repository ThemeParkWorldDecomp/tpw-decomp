// Test library for the progress bar (unit v5697; libtpw_unit_CreateUIProgressBar.dylib, tests/test_CreateUIProgressBar.py), main app:
//   src/InterfaceWindow_virtuals_CreateUIProgressBar.cpp   CreateUIProgressBar (two), UpdateUIProgressBar; defines gUIProgressBar and gUIProgressBarText
//
// The callees are recording fakes with the PowerPC stubs' event names. The windows are raw blocks whose vtable pointer is a hand-made table
// (no InterfaceWindow constructor is linked): Enable, SetValue and SetText are the only virtuals called. The two draw object constructors are
// fakes under their mangled names, so that no draw object class has to be linked.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gUIRootWindow;   // 0x10395c24 (src/UI_focus.cpp)
InterfaceWindow* gUIProgressBar;   // 0x10395c6c (src/ui_progress_cursor.cpp)
int16_t gProgBarScript[8];        // 0x102afe24
char gProgBarMeshName[16] = "progress.wct";   // 0x1024eda3

namespace {

constexpr size_t kWinSize = 1024;
static_assert(sizeof(InterfaceWindow) <= kWinSize, "fake window block too small");
alignas(16) uint8_t gWin[3][kWinSize];   // the root, the bar, the child 0x2241
alignas(16) uint8_t gFontBlock[64];
char16_t gTextBuf[64];
void* gVt[32];

InterfaceWindow* Win(int i) { return reinterpret_cast<InterfaceWindow*>(gWin[i]); }

void EvText(const char* name, const void* self, const char16_t* text) {
    std::string t = narrow(text);
    ev(name, {lab(self)}, {t.c_str()});
}

int16_t FakeEnable(InterfaceWindow* self, int16_t on) {
    ev("Enable", {lab(self), on});
    return 0;
}
int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}
int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    EvText("SetText", self, text);
    return 1;
}

void InitFakes() {
    // Itanium vtable order: DrawFunction, CalcClientArea, D1, D0, WhatAmI, ClientDraw, Enable, operator-, SetValue, GetValue, SetText.
    gVt[6] = reinterpret_cast<void*>(&FakeEnable);
    gVt[8] = reinterpret_cast<void*>(&FakeSetValue);
    gVt[10] = reinterpret_cast<void*>(&FakeSetText);
    for (int i = 0; i < 3; ++i) *reinterpret_cast<void***>(gWin[i]) = gVt;
}

struct Init {
    Init() { InitFakes(); }
} gInit;

}  // namespace

// ---- the draw object constructors, under their mangled names
extern "C" void FakeHGradientCtor(void* self, void* mesh) asm("__ZN28InterfaceDrawEngineHGradientC1EPv");
extern "C" void FakeHGradientCtor(void* self, void* mesh) { ev("HGradientCtor", {newlab(self), lab(mesh)}); }
extern "C" void FakeTextSpriteCtor(void* self, void* data) asm("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeTextSpriteCtor(void* self, void* data) { ev("TextSpriteCtor", {newlab(self), lab(data)}); }

// ---- the other callees
int32_t ProgBarMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<void*>(callback))});
    return Win(1);
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Win(2);
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetClientDrawText(char16_t* text) { EvText("SetClientDrawText", this, text); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFontBlock);
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return gTextBuf;
}

extern "C" {

uint64_t tpw_cpb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"root", gWin[0]},          {"bar", gWin[1]},          {"child", gWin[2]},        {"font", gFontBlock},
        {"text", gTextBuf},         {"script", gProgBarScript}, {"mesh", gProgBarMeshName},
        {"callback", reinterpret_cast<const void*>(&ProgBarMsg)}, {"textval", &gUIProgressBarText},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cpb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cpb_layout(int64_t*) { return 0; }

// bar: 0 no bar, 1 the bar block, 2 the child block (any other window).
void tpw_cpb_setup(int32_t bar, int32_t textval) {
    gUIRootWindow = Win(0);
    gUIProgressBar = bar == 0 ? nullptr : (bar == 1 ? Win(1) : Win(2));
    gUIProgressBarText = textval;
}

int32_t tpw_cpb_state() {
    if (gUIProgressBar == nullptr) return 0;
    if (gUIProgressBar == Win(1)) return 1;
    if (gUIProgressBar == Win(2)) return 2;
    return 9;
}

void tpw_cpb_big(uint64_t text) { CreateUIProgressBar(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text))); }
void tpw_cpb_mid(int32_t text) { CreateUIProgressBar(text); }
void tpw_cpb_upd(int32_t percent) { UpdateUIProgressBar(percent); }

}  // extern "C"
