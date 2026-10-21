// Test library for VisitorStatusPanel::InitStatusWindow (unit v5521; libtpw_unit_VisitorStatusPanel_InitStatusWindow.dylib,
// tests/test_VisitorStatusPanel_InitStatusWindow.py), main app:
//   src/WorldSavvy_VisitorStatusPanel_InitStatusWindow.cpp   VisitorStatusPanel::InitStatusWindow
//
// Every callee is a fake that records what the real one reads. The windows are small blocks whose first word is their id; the objects the
// fake operator new makes carry their number (the count of news in the case) in the first word, so the log names them.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

namespace {

uint64_t gWin[16];          // window blocks by id - 0x3724 (host pointers)
uint32_t gFont[16];         // font table: word i = 0xF000 + i
char16_t gText[256][2];     // string i: one character 0x4100 + i
int32_t gNews = 0;

int64_t Wid(const void* w) { return w ? static_cast<int64_t>(*static_cast<const uint32_t*>(w)) : -1; }
int64_t Mark(const void* o) { return o ? static_cast<int64_t>(*static_cast<const uint32_t*>(o)) : 0; }

}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Wid(this), id});
    if (id == 0x3732 && ret("Win", id, 0) != 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(gWin[id - 0x3724]));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {Wid(this), Mark(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {Wid(this), Mark(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {Wid(this), text ? static_cast<int64_t>(text[0]) : -1}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(&gFont[index & 15]);
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return gText[id & 255];
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("UIMain_IndentClient", {Wid(window), a, b}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    int64_t null = ret("New");
    int32_t n = ++gNews;
    ev("OperatorNew", {size, null != 0});
    if (null) return nullptr;
    void* p = tpw::ArenaAllocZeroed(size);
    *static_cast<uint32_t*>(p) = static_cast<uint32_t>(n);
    return p;
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {Mark(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {Mark(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {Mark(mem)}); }
void PercentCtor(void* mem) { ev("PercentCtor", {Mark(mem)}); }
void WindowCtor(void*, InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {Wid(window), value}); }
}  // namespace lender_callee

namespace vsp_callee {
void GradientCtor(void* mem, void* data) {
    uint32_t h = 0;
    for (const char* c = static_cast<const char*>(data); *c; ++c) h = h * 31 + static_cast<uint8_t>(*c);
    ev("GradientCtor", {Mark(mem), h});
}
void TimeCtor(void* mem) { ev("TimeCtor", {Mark(mem)}); }
}  // namespace vsp_callee

// Defines the symbol the src file refers to for the real HGradient constructor (never called: GradientCtor is faked above), so the library links.
extern "C" void VspFakeGradientCtor(void*, void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
extern "C" void VspFakeGradientCtor(void*, void*) {}

extern "C" {

uint64_t tpw_vsp_addr(const char* name) {
    if (std::strcmp(name, "wintab") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWin));
    return 0;
}

uint64_t tpw_vsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena's base: the panel's window is a Ptr32 (an arena offset) since unit v5L31.
uint64_t tpw_vsp_arena_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_vsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(VisitorStatusPanel, mWindow));
    return n;
}

void tpw_vsp_reset() {
    gNews = 0;
    for (int i = 0; i < 16; i++) gFont[i] = 0xF000 + i;
    for (int i = 0; i < 256; i++) {
        gText[i][0] = static_cast<char16_t>(0x4100 + i);
        gText[i][1] = 0;
    }
}

void tpw_vsp_InitStatusWindow(uint64_t panel) { reinterpret_cast<VisitorStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow(); }

}  // extern "C"
