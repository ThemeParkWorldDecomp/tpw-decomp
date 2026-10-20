// Test library for InterfaceListControl2 FindData, PropagateMessage, SelectClick, SetCurrent, SetSort and ColumnButtonClick (unit v5908;
// libtpw_unit_InterfaceListControl2_SetSort.dylib, tests/test_InterfaceListControl2_SetSort.py), main app:
//   src/InterfaceListControl2_items_InterfaceListControl2_SetSort.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: UI_SendMessage,
// UI_PostMessage, the list's AdvanceIndex, ChangeCurEntry, UpdateDisplay and Sort, the window's GetWindow, GetChild and GetNext, the rectangle's
// PointIn and the scroll bar's SetValue (WindowSetValue). The list object is raw zeroed memory (the constructor is not run); its rectangle
// gets the fake CoolRect vtable.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"

using namespace calllog;

void WindowSetValue(InterfaceWindow* window, int32_t value);

namespace {
char* gBar = nullptr;   // what GetWindow answers
}

// ---- the rectangle classes (their real code is in files this library does not compile)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect(int16_t left, int16_t top, int16_t right, int16_t bottom) : mLeft(left), mTop(top), mRight(right), mBottom(bottom) {}
CoolRect::~CoolRect() {}
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRect::PointIn(int16_t x, int16_t y) {
    ev("PointIn", {x, y});
    return ret("PointIn") != 0;
}

// ---- the fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Send", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Send"));
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Post", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Post"));
}

void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(gBar);
}

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return reinterpret_cast<InterfaceWindow*>(reinterpret_cast<char*>(this) + 0x20);
}

InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    return reinterpret_cast<InterfaceWindow*>(reinterpret_cast<char*>(this) + 0x40);
}

int32_t InterfaceListControl2::AdvanceIndex(int32_t index, int32_t* change) {
    ev("Advance", {lab(this), index, change != nullptr ? 1 : 0});
    return static_cast<int32_t>(ret("Advance"));
}

void InterfaceListControl2::ChangeCurEntry(int32_t row) { ev("ChangeCur", {lab(this), row}); }

void InterfaceListControl2::UpdateDisplay() { ev("Update", {lab(this)}); }

void InterfaceListControl2::Sort(int32_t unused) { ev("Sort", {lab(this), unused}); }

extern "C" {

uint64_t tpw_ilc2s_addr(const char*) { return 0; }

uint64_t tpw_ilc2s_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The list object: zeroed memory with the rectangle's vtable.
uint64_t tpw_ilc2s_new_object() {
    InterfaceListControl2* object = static_cast<InterfaceListControl2*>(tpw::ArenaAllocZeroed(sizeof(InterfaceListControl2)));
    new (&object->mListRect) CoolRect(0, 0, 0, 0);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

void tpw_ilc2s_set_bar(uint64_t bar) { gBar = reinterpret_cast<char*>(static_cast<uintptr_t>(bar)); }

// The host offsets of the fields the module writes (the Mac's are in the module).
int32_t tpw_ilc2s_layout(int64_t* out) {
    char* base = static_cast<char*>(tpw::ArenaAllocZeroed(sizeof(InterfaceListControl2)));
    InterfaceListControl2* o = reinterpret_cast<InterfaceListControl2*>(base);
    auto off = [&](const void* member) { return static_cast<int64_t>(reinterpret_cast<const char*>(member) - base); };
    CoolRect* rect = reinterpret_cast<CoolRect*>(base);
    int32_t n = 0;
    out[n++] = off(&o->mAttributes);
    out[n++] = off(&o->mParent);
    out[n++] = off(&o->mId);
    out[n++] = off(&o->mListRect.mTop);
    out[n++] = off(&o->mItemStorage);
    out[n++] = off(&o->mItemWindows);
    out[n++] = off(&o->mCurrentEntry);
    out[n++] = off(&o->mItemCount);
    out[n++] = off(&o->mTopEntry);
    out[n++] = off(&o->mFirstEntry);
    out[n++] = off(&o->mColumnCount);
    out[n++] = off(&o->mSortColumn);
    out[n++] = off(&o->mSortDescending);
    out[n++] = off(&o->mColumns);
    out[n++] = off(&o->mRowHeight);
    out[n++] = off(&o->mVisibleRows);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceListControl2));
    out[n++] = static_cast<int64_t>(sizeof(CoolRect));
    out[n++] = off(&rect->mLeft);
    out[n++] = off(&rect->mRight);
    return n;
}

int32_t tpw_ilc2s_FindData(uint64_t object, int32_t data) {
    return reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->FindData(data);
}
void tpw_ilc2s_PropagateMessage(uint64_t object, int32_t message, int32_t a, int32_t b) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->PropagateMessage(message, a, b);
}
void tpw_ilc2s_SelectClick(uint64_t object, int32_t x, int32_t y) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->SelectClick(static_cast<int16_t>(x), static_cast<int16_t>(y));
}
void tpw_ilc2s_SetCurrent(uint64_t object, int32_t position) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->SetCurrent(position);
}
void tpw_ilc2s_SetSort(uint64_t object, int32_t column, int32_t sortNow) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->SetSort(column, sortNow != 0);
}
void tpw_ilc2s_ColumnButtonClick(uint64_t object, int32_t id) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->ColumnButtonClick(id);
}

}  // extern "C"
