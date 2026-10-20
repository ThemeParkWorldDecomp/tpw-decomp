// Test library for ShowStatusPanel::InitStatusWindow (unit v51018; libtpw_unit_ShowStatusPanel_InitStatusWindow.dylib,
// tests/test_ShowStatusPanel_InitStatusWindow.py), main app:
//   src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_InitStatusWindow.cpp   InitStatusWindow and gStatusMeshNames
//
// The callees outside the unit are fakes that record what the real ones read.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

namespace {
char gKids[16][0x200];
char gFont[16];
char16_t gStrings[0x100][4];
}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int32_t index = id - 0xa092;
    if (index < 0 || index >= 16) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(gKids[index]);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetObj", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetText", {lab(this), text ? static_cast<int64_t>(text[0]) : -1}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("Font", {static_cast<int32_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFont);
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    char16_t* s = gStrings[id & 0xff];
    s[0] = static_cast<char16_t>(0x100 + id);
    s[1] = 0x41;
    s[2] = 0;
    return s;
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("Indent", {lab(window), a, b}); }

// The unit's weak callee definitions name these constructors; they are never reached (the strong fakes below replace the callees).
// Defined as plain functions under the constructors' symbols so that no vtable is pulled in.
extern "C" {
void FakeSpriteCtorSym(void*) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeCashCtorSym(void*) __asm__("__ZN23InterfaceCashTextObjectC1EPv");
void FakeGradCtorSym(void*, void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void FakeSpriteCtorSym(void*) {}
void FakeCashCtorSym(void*) {}
void FakeGradCtorSym(void*, void*) {}
}

namespace statusinit_callee {
void* OperatorNew(uint32_t size) {
    ev("New", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void GradientCtor(void* mem, const char* meshName) {
    ev("GradMesh", {static_cast<int64_t>(meshName - gStatusMeshNames)});
    ev("GradCtor", {newlab(mem)});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace statusinit_callee

extern "C" {

uint64_t tpw_ssp_addr(const char* name) {
    if (std::strcmp(name, "font") == 0) return reinterpret_cast<uintptr_t>(gFont);
    if (std::strncmp(name, "kid", 3) == 0) return reinterpret_cast<uintptr_t>(gKids[std::atoi(name + 3)]);
    return 0;
}

uint64_t tpw_ssp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit form of an arena address (what a Ptr32 holds).
uint32_t tpw_ssp_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_ssp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    return n;
}

void tpw_ssp_init(uint64_t panel) { reinterpret_cast<ShowStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow(); }

}  // extern "C"
