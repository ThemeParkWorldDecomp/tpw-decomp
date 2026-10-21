// Test library for FeatureStatusPanel::InitStatusWindow (unit v5676; libtpw_unit_FeatureStatusPanel_InitStatusWindow.dylib,
// tests/test_FeatureStatusPanel_InitStatusWindow.py), main app:
//   src/game/ui/ui_FeatureStatus.cpp
//
// The callees are fakes that record the arguments the real ones read. Windows are blocks inside the parent block, so they are logged
// as offsets from the parent.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "MainWindowCallback.h"
#include "ui_main.h"

using namespace calllog;

namespace {
uint8_t* gParent = nullptr;

int64_t Rel(const void* p) { return static_cast<int64_t>(static_cast<int32_t>(reinterpret_cast<const uint8_t*>(p) - gParent)); }
}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    gParent = reinterpret_cast<uint8_t*>(this);
    ev("GetWindow", {id});
    if (ret("GetWindow") == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(gParent + 0x200 * (id - 0x1583));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {Rel(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {Rel(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(font))}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {Rel(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(ret("GetFontTableEntry")));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("UIMain_GetString")));
}

namespace allitems_post_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    if (ret("new") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) {
    newlab(mem);
    ev("SpriteCtor", {lab(mem)});
}
void NumberCtor(void* mem) {
    newlab(mem);
    ev("NumberCtor", {lab(mem)});
}
void CashCtor(void* mem) {
    newlab(mem);
    ev("CashCtor", {lab(mem)});
}
void GradientCtor(void*, const char*) {}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {Rel(window), value}); }
}  // namespace allitems_post_callee

extern "C" {

uint64_t tpw_fspinit_addr(const char*) { return 0; }

uint64_t tpw_fspinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fspinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    return n;
}

void tpw_fspinit_init(uint64_t panel) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow(); }

}  // extern "C"
