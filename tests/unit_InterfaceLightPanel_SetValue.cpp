// Test library for InterfaceLightPanel::SetValue (unit v5770; libtpw_unit_InterfaceLightPanel_SetValue.dylib), main app:
//   src/ui_whatami_InterfaceLightPanel_SetValue.cpp
//
// InterfaceWindow::GetWindow, SetFrame and SetValue are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"

using namespace calllog;

namespace {
char gDummyWindow[16];
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") ? reinterpret_cast<InterfaceWindow*>(gDummyWindow) : nullptr;
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }

int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}

extern "C" {

uint64_t tpw_lp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// [host offset of mValue, sizeof]
int32_t tpw_lp_layout(int64_t* out) {
    int32_t n = 0;
    InterfaceLightPanel* p = reinterpret_cast<InterfaceLightPanel*>(tpw::ArenaAllocZeroed(sizeof(InterfaceLightPanel)));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&p->mValue) - reinterpret_cast<char*>(p));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceLightPanel));
    return n;
}

int32_t tpw_lp_set_value(uint64_t obj, int32_t value) {
    return reinterpret_cast<InterfaceLightPanel*>(obj)->InterfaceLightPanel::SetValue(value);
}

}  // extern "C"
