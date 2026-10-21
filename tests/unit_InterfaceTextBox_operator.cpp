// Test library for InterfaceTextBox::operator- (unit v5712; libtpw_unit_InterfaceTextBox_operator.dylib), main app:
//   src/game/ui/InterfaceTextBox.cpp
//
// The host window is an arena block whose vtable pointer is that of a Probe (same slot for WhatAmI as InterfaceWindow); the Probe's
// WhatAmI records an event and answers the word the test put in mF144. wcscmp is a recording fake.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

namespace {

// Same virtual order as InterfaceWindow up to WhatAmI: DrawFunction, CalcClientArea, destructor, WhatAmI.
struct Probe {
    virtual void A() {}
    virtual void B() {}
    virtual ~Probe() {}
    virtual int32_t W();
};

#pragma clang diagnostic ignored "-Winvalid-offsetof"
const size_t kKindOffset = offsetof(InterfaceTextBox, mF144);

int32_t Probe::W() {
    int32_t kind = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(this) + kKindOffset);
    ev("WhatAmI", {kind});
    return kind;
}

}  // namespace

int32_t wcscmp(const char16_t* a, const char16_t* b) {
    auto sa = narrow(a);
    auto sb = narrow(b);
    ev("wcscmp", {}, {sa.c_str(), sb.c_str()});
    return static_cast<int32_t>(ret("wcscmp"));
}

extern "C" {

uint64_t tpw_itb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window block with the probe's vtable pointer.
uint64_t tpw_itb_window() {
    void* block = tpw::ArenaAllocZeroed(sizeof(InterfaceTextBox));
    new (block) Probe();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

// mText offset, mF144 offset.
int32_t tpw_itb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTextBox, mText));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTextBox, mF144));
    return n;
}

uint32_t tpw_itb_minus(uint64_t self, uint64_t other) {
    InterfaceTextBox* a = reinterpret_cast<InterfaceTextBox*>(static_cast<uintptr_t>(self));
    InterfaceWindow* b = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(other));
    return static_cast<uint32_t>(a->InterfaceTextBox::operator-(*b));
}

}  // extern "C"
