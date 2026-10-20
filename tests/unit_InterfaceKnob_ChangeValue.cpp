// Test library for InterfaceKnob::ChangeValue (unit v5264; libtpw_unit_InterfaceKnob_ChangeValue.dylib, tests/test_InterfaceKnob_ChangeValue.py):
//   src/InterfaceKnob_InterfaceKnob_ChangeValue.cpp
//
// UI_PostMessage is a recording fake. The client draw object is a fake class with the same vtable slot for Change as
// InterfaceDrawObject (destructor, two Draws, Change), recording the flags and the angle the real one would be told.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "UI_Base.h"

using namespace calllog;

namespace {

struct FakeDO {
    virtual ~FakeDO() {}
    virtual void Draw1() {}
    virtual void Draw2() {}
    virtual void Change(uint32_t flags, DOChange* change) {
        ev("Change", {lab(this), static_cast<int64_t>(flags), static_cast<int64_t>(change->mF08), 0});
    }
};

}  // namespace

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}

extern "C" {

uint64_t tpw_ik_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ik_newknob() { return tpw_ik_alloc(sizeof(InterfaceKnob)); }

uint64_t tpw_ik_newdo() {
    void* p = tpw::ArenaAllocZeroed(sizeof(FakeDO));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (p) FakeDO()));
}

// f: minAngle, maxAngle, angleRange, dragAngle, angle, minValue, maxValue, valueRange, value, oldValue
void tpw_ik_set(uint64_t knob, uint64_t parent, int32_t id, uint64_t dobj, const int32_t* f) {
    InterfaceKnob* k = reinterpret_cast<InterfaceKnob*>(knob);
    k->mParent = reinterpret_cast<InterfaceWindow*>(parent);
    k->mId = id;
    k->mClientDrawObject = reinterpret_cast<InterfaceDrawObject*>(dobj);
    k->mMinAngle = f[0];
    k->mMaxAngle = f[1];
    k->mAngleRange = f[2];
    k->mDragAngle = f[3];
    k->mAngle = f[4];
    k->mMinValue = f[5];
    k->mMaxValue = f[6];
    k->mValueRange = f[7];
    k->mValue = f[8];
    k->mOldValue = f[9];
}

void tpw_ik_get(uint64_t knob, int32_t* f) {
    InterfaceKnob* k = reinterpret_cast<InterfaceKnob*>(knob);
    f[0] = k->mMinAngle;
    f[1] = k->mMaxAngle;
    f[2] = k->mAngleRange;
    f[3] = k->mDragAngle;
    f[4] = k->mAngle;
    f[5] = k->mMinValue;
    f[6] = k->mMaxValue;
    f[7] = k->mValueRange;
    f[8] = k->mValue;
    f[9] = k->mOldValue;
}

int32_t tpw_ik_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(DOChange, mF08));
    return n;
}

void tpw_ik_ChangeValue(uint64_t knob, int32_t value) { reinterpret_cast<InterfaceKnob*>(knob)->ChangeValue(value); }

}  // extern "C"
