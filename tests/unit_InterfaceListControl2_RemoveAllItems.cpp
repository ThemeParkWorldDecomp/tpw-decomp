// Test library for InterfaceListControl2::RemoveAllItems / RemoveItem / RemoveMe (unit v5095;
// libtpw_unit_InterfaceListControl2_RemoveAllItems.dylib, tests/test_InterfaceListControl2_RemoveAllItems.py), main app:
//   src/game/ui/InterfaceListControl2.cpp
//
// The list object is raw zeroed memory (never constructed). The callees outside the unit are fakes that record what the real ones read:
// AdvanceIndex and FindFreeEntry are small deterministic stand-ins over the item block, the windows' virtuals (scroll bar SetValue,
// draw object Show) go through hand-made vtables.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceScrollBar.h"
#include "UI_Base.h"
#include "ui_controls.h"

using namespace calllog;

int32_t gUIAutoRedraw;

namespace {

InterfaceWindow* gBar = nullptr;
void* gWinVt[128];
void* gDoVt[128];

int32_t FakeSetValue(InterfaceWindow* window, int32_t value) {
    ev("SetValue", {lab(window), value});
    return 0;
}
void FakeDoShow(InterfaceDrawObject* object, int16_t shown) { ev("DOShow", {lab(object), shown}); }
void Nothing() {}

template <typename M>
size_t SlotOf(M member) {
    uintptr_t pair[2];
    static_assert(sizeof(M) == 16, "member pointer");
    std::memcpy(pair, &member, sizeof(M));
    uintptr_t offset = (pair[0] & 1) ? pair[0] - 1 : pair[0];
    return offset / sizeof(void*);
}

void MakeVtables() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int i = 0; i < 128; ++i) {
        gWinVt[i] = reinterpret_cast<void*>(&Nothing);
        gDoVt[i] = reinterpret_cast<void*>(&Nothing);
    }
    gWinVt[SlotOf(&InterfaceWindow::SetValue)] = reinterpret_cast<void*>(&FakeSetValue);
    gDoVt[SlotOf(&InterfaceDrawObject::Show)] = reinterpret_cast<void*>(&FakeDoShow);
}

}  // namespace

// ---- the fakes
namespace online_callee {
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {lab(self), flag}); }
}  // namespace online_callee

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return gBar;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), min, max}); }

// The entry after `index`: its child, else its next, else the next of the nearest ancestor that has one, else -1.
int32_t InterfaceListControl2::AdvanceIndex(int32_t index, int32_t* change) {
    ev("AdvanceIndex", {lab(this), index, change != nullptr ? 1 : 0});
    int32_t child = Words()[index + kLCChild].mInt;
    if (child >= 0) return child;
    int32_t next = Words()[index].mInt;
    if (next >= 0) return next;
    int32_t parent = Words()[index + kLCParent].mInt;
    for (int n = 0; parent >= 0 && n < 64; ++n) {
        next = Words()[parent].mInt;
        if (next >= 0) return next;
        parent = Words()[parent + kLCParent].mInt;
    }
    return -1;
}
void InterfaceListControl2::ChangeCurEntry(int32_t row) { ev("ChangeCurEntry", {lab(this), row}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }
// The first entry of the chain from mFirstEntry whose parent word is -2, and the one before it (at most 64 steps).
void InterfaceListControl2::FindFreeEntry(int32_t& freeEntry, int32_t& previous) {
    ev("FindFreeEntry", {lab(this)});
    int32_t found = -1, before = -1, entry = mFirstEntry;
    for (int n = 0; entry != -1 && n < 64; ++n) {
        if (Words()[entry + kLCParent].mInt == -2) {
            found = entry;
            break;
        }
        before = entry;
        entry = Words()[entry].mInt;
    }
    freeEntry = found;
    previous = before;
}

extern "C" {

uint64_t tpw_ilc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The vptr to store at offset 0 of a fake window (kind 0: SetValue logs) or draw object (kind 1: Show logs).
uint64_t tpw_ilc_vptr(int32_t kind) {
    MakeVtables();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(kind == 0 ? gWinVt : gDoVt));
}

void tpw_ilc_setbar(uint64_t bar) { gBar = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(bar)); }
void tpw_ilc_setauto(int32_t value) { gUIAutoRedraw = value; }

// The host offsets: sizeof(list), storage, current, count, top, blockwords, first, columns, columnarray, visible, drawobject, parent,
// sizeof(CoolRect), offset of its bottom.
int32_t tpw_ilc_layout(int64_t* out) {
    alignas(16) static char buf[1024];
    auto* o = reinterpret_cast<InterfaceListControl2*>(buf);
    auto* cr = reinterpret_cast<CoolRect*>(buf);
    auto off = [](const void* p) { return static_cast<int64_t>(reinterpret_cast<const char*>(p) - buf); };
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceListControl2));
    out[n++] = off(&o->mItemStorage);
    out[n++] = off(&o->mCurrentEntry);
    out[n++] = off(&o->mItemCount);
    out[n++] = off(&o->mTopEntry);
    out[n++] = off(&o->mBlockWords);
    out[n++] = off(&o->mFirstEntry);
    out[n++] = off(&o->mColumnCount);
    out[n++] = off(&o->mColumns);
    out[n++] = off(&o->mVisibleRows);
    out[n++] = off(&o->mItemDrawObject);
    out[n++] = off(&o->mParent);
    out[n++] = static_cast<int64_t>(sizeof(CoolRect));
    out[n++] = off(&cr->mBottom);
    return n;
}

void tpw_ilc_remove_all(uint64_t list) { reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))->RemoveAllItems(); }
int32_t tpw_ilc_remove_item(uint64_t list, int32_t position) {
    return reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))->RemoveItem(position);
}
void tpw_ilc_remove_me(uint64_t list, int32_t position, int32_t entry, int32_t previous, int32_t flag) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))->RemoveMe(position, entry, previous, flag);
}

}  // extern "C"
