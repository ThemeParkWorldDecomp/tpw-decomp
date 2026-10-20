// Test library for InterfaceVaryBox::ChangeValue and SetMinMaxValues (unit v5266; libtpw_unit_InterfaceVaryBox_ChangeValue.dylib), main app:
//   src/ui_whatami_InterfaceVaryBox_ChangeValue.cpp
//
// InterfaceWindow::SetValue and UI_PostMessage are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"

using namespace calllog;

int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}

extern "C" {

uint64_t tpw_vb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_vb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mMinValue));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mMaxValue));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mValue));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mOldValue));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mForceUpdate));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mParent));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceVaryBox, mId));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceVaryBox));
    return n;
}

uint32_t tpw_vb_ChangeValue(uint64_t self, int32_t value) {
    return static_cast<uint32_t>(reinterpret_cast<InterfaceVaryBox*>(static_cast<uintptr_t>(self))->ChangeValue(value));
}

void tpw_vb_SetMinMaxValues(uint64_t self, uint64_t minimum, uint64_t maximum) {
    reinterpret_cast<InterfaceVaryBox*>(static_cast<uintptr_t>(self))
        ->SetMinMaxValues(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(minimum)), reinterpret_cast<int32_t*>(static_cast<uintptr_t>(maximum)));
}

}  // extern "C"
