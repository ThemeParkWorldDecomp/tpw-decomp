// Test library for the outbox panel's ColumnClick (unit v5374; libtpw_unit_ColumnClick.dylib, tests/test_ColumnClick.py), main app:
//   src/UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg_ColumnClick.cpp   ColumnClick
//
// The window lookup and the list's item functions are recording fakes; gOutboxWindow is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "ui_controls.h"

using namespace calllog;

InterfaceWindow* gOutboxWindow;

namespace {
InterfaceWindow* gList;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return gList;
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 1;
}

void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {lab(this), position, item[2].mInt, item[3].mInt, columns, update, unused});
}

void InterfaceListControl2::SetData(int32_t position, int32_t data) {
    ev("SetData", {lab(this), position, data});
}

extern "C" {

uint64_t tpw_cc_addr(const char* name) {
    if (std::strcmp(name, "win") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOutboxWindow));
    return 0;
}

uint64_t tpw_cc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_cc_setlist(uint64_t p) { gList = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(p)); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int32_t tpw_cc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceListControl2, mCurrentEntry));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceListControl2));
    return n;
}
#pragma clang diagnostic pop

void tpw_cc_click(int32_t column) { ColumnClick(column); }

}  // extern "C"
