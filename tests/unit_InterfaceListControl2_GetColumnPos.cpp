// Test library for the list control's GetColumnPos, GetItem, SetData, GetNonHierIndexedEntry and default message function (unit v5955;
// libtpw_unit_InterfaceListControl2_GetColumnPos.dylib, tests/test_InterfaceListControl2_GetColumnPos.py), main app:
//   src/InterfaceListControl2_items_InterfaceListControl2_GetColumnPos.cpp   the five functions and gListControl2Keys
//
// The callees outside the unit (AdvanceIndex, the list's other members, the window's members, the posts and the default function) are
// recording fakes. The virtual calls (the rectangles' PointIn and Shift, the scroll bar's SetValue and GetValue, the draw object's Show)
// go through hand-made vtables whose slots are found from pointers to the members; the fakes record the same events as the PowerPC
// side's glue stub. gUIAutoRedraw is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

int32_t gUIAutoRedraw;

namespace {

InterfaceWindow* gBar = nullptr;

bool FakePointIn(CoolRect* self, int16_t x, int16_t y) {
    int64_t label = lab(self);
    ev("PointIn", {label, x, y});
    return ret(label == 31 ? "PointInC" : "PointInL") != 0;
}
void FakeShift(CoolRect* self, int16_t dx, int16_t dy) { ev("Shift", {lab(self), dx, dy}); }
int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}
int32_t FakeGetValue(InterfaceWindow* self) {
    ev("GetValue", {lab(self)});
    return static_cast<int32_t>(ret("GetValue"));
}
void FakeDrawShow(InterfaceDrawObject* self, int16_t shown) { ev("DOShow", {lab(self), shown}); }

// The vtable slot (counted from the address point) of a virtual member, from the pointer to it.
template <class PM>
size_t SlotOf(PM pm) {
    uintptr_t words[2];
    std::memcpy(words, &pm, sizeof words);
    return words[0] / sizeof(void*);   // arm64: the pointer is the offset itself (the virtual flag is in the adjustment's low bit)
}

void* gRectTable[32];
void* gWindowTable[32];
void* gDrawTable[32];

}  // namespace

// ---- the fakes
bool CoolRect::PointIn(int16_t x, int16_t y) { return FakePointIn(this, x, y); }
void CoolRect::Shift(int16_t dx, int16_t dy) { FakeShift(this, dx, dy); }

int32_t InterfaceListControl2::AdvanceIndex(int32_t index, int32_t* change) {
    ev("AdvanceIndex", {lab(this), index, change != nullptr ? 1 : 0});
    return static_cast<int32_t>(ret("AdvanceIndex"));
}
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }
void InterfaceListControl2::ColumnButtonClick(int32_t id) { ev("ColumnButtonClick", {lab(this), id}); }
void InterfaceListControl2::ChangeCurEntry(int32_t row) { ev("ChangeCurEntry", {lab(this), row}); }
void InterfaceListControl2::PropagateMessage(int32_t message, int32_t a, int32_t b) { ev("PropagateMessage", {lab(this), message, a, b}); }
void InterfaceListControl2::SelectClick(int16_t x, int16_t y) { ev("SelectClick", {lab(this), x, y}); }
void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), position}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? gBar : nullptr;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
uint32_t InterfaceWindow::GetAttr() {
    ev("GetAttr", {lab(this)});
    return static_cast<uint32_t>(ret("GetAttr"));
}
int32_t InterfaceWindow::CaptureKeys(captureKeyList* keys) { ev("CaptureKeys", {lab(this), lab(keys)}); return 0; }  // int32_t since v51111

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_DefaultWindowCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ilc2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"auto", &gUIAutoRedraw},
        {"keys", gListControl2Keys},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ilc2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The scroll bar window GetWindow answers.
void tpw_ilc2_setbar(uint64_t bar) { gBar = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(bar)); }

// Gives the object (kind 0 a rectangle, 1 a window, 2 a draw object) the vtable of fakes.
void tpw_ilc2_vt(uint64_t object, int32_t kind) {
    if (gRectTable[SlotOf(&CoolRect::PointIn)] == nullptr) {
        gRectTable[SlotOf(&CoolRect::PointIn)] = reinterpret_cast<void*>(&FakePointIn);
        gRectTable[SlotOf(&CoolRect::Shift)] = reinterpret_cast<void*>(&FakeShift);
        gWindowTable[SlotOf(&InterfaceWindow::SetValue)] = reinterpret_cast<void*>(&FakeSetValue);
        gWindowTable[SlotOf(&InterfaceWindow::GetValue)] = reinterpret_cast<void*>(&FakeGetValue);
        gDrawTable[SlotOf(&InterfaceDrawObject::Show)] = reinterpret_cast<void*>(&FakeDrawShow);
    }
    void** table = kind == 0 ? gRectTable : kind == 1 ? gWindowTable : gDrawTable;
    *reinterpret_cast<void***>(static_cast<uintptr_t>(object)) = table;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ilc2_layout(int64_t* out) {
    int32_t n = 0;
    using L = InterfaceListControl2;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mClientRect));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mAttributes));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mParent));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mId));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mButtonsHeld));
    out[n++] = static_cast<int64_t>(offsetof(L, mListRect));
    out[n++] = static_cast<int64_t>(offsetof(L, mItemStorage));
    out[n++] = static_cast<int64_t>(offsetof(L, mItemWindows));
    out[n++] = static_cast<int64_t>(offsetof(L, mCurrentEntry));
    out[n++] = static_cast<int64_t>(offsetof(L, mItemCount));
    out[n++] = static_cast<int64_t>(offsetof(L, mTopEntry));
    out[n++] = static_cast<int64_t>(offsetof(L, mFirstEntry));
    out[n++] = static_cast<int64_t>(offsetof(L, mColumnCount));
    out[n++] = static_cast<int64_t>(offsetof(L, mColumns));
    out[n++] = static_cast<int64_t>(offsetof(L, mRowHeight));
    out[n++] = static_cast<int64_t>(offsetof(L, mVisibleRows));
    out[n++] = static_cast<int64_t>(offsetof(L, mItemDrawObject));
    out[n++] = static_cast<int64_t>(sizeof(L));
    out[n++] = static_cast<int64_t>(sizeof(CoolRect));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mTop));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mRight));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mBottom));
    out[n++] = static_cast<int64_t>(offsetof(LCStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(LCStringRep));
    out[n++] = static_cast<int64_t>(sizeof(LCWord));
    return n;
}

// ---- the functions
uint32_t tpw_ilc2_colpos(uint64_t list, int32_t column, uint64_t left, uint64_t right) {
    return reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))
               ->GetColumnPos(column, reinterpret_cast<int16_t*>(static_cast<uintptr_t>(left)),
                              reinterpret_cast<int16_t*>(static_cast<uintptr_t>(right)))
               ? 1u : 0u;
}
uint32_t tpw_ilc2_getitem(uint64_t list, int32_t position, int32_t column, uint64_t out) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))
        ->GetItem(position, column, *reinterpret_cast<UI_LCAdd*>(static_cast<uintptr_t>(out)));
    return 0u;
}
uint32_t tpw_ilc2_setdata(uint64_t list, int32_t position, int32_t data) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))->SetData(position, data);
    return 0u;
}
int32_t tpw_ilc2_nonhier(uint64_t list, int32_t position, int32_t parent) {
    return reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))->GetNonHierIndexedEntry(position, parent);
}
int32_t tpw_ilc2_callback(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return UI_DefaultListControl2Callback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
