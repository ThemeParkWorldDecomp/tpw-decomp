// Test library for the online contacts panel's receive-string, list handler and buddy helpers (unit v5991;
// libtpw_unit_OnlineReceiveString_2.dylib, tests/test_OnlineReceiveString_2.py), main app:
//   src/game/ui/ui_onlinePanel.cpp   OnlineReceiveString, ContactListHandler, _OnlineAddBuddy, _OnlineRemoveBuddy, _OnlineUpdateBuddy
//
// The callees outside the unit are recording fakes (same event names and arguments as the module's PowerPC stubs). The list controls are
// raw arena blocks: the fakes keep the item count (mItemCount) the way the real AddItem / RemoveItem do.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "online.h"
#include "ui_controls.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "UI_Base.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gContactsFontIndex = 7;
int32_t gOnlineContactsBusy = 0;
InterfaceWindow* gOnlinePanel = nullptr;
COnlineControl* gOnlineControl = nullptr;
int16_t gUIRealScreenWidth = 0;

namespace {
InterfaceWindow* g_panel = nullptr;
InterfaceWindow* g_win = nullptr;
InterfaceWindow* g_list = nullptr;
const char16_t* g_base = nullptr;
char g_fontDummy[64];

int64_t HashText(const char16_t* s, int64_t* len) {
    int64_t h = 0, n = 0;
    while (s[n] != 0) {
        h = (h * 31 + s[n]) & 0x7fffffff;
        n++;
    }
    *len = n;
    return h;
}
}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (this == g_panel) return g_win;
    if (this == g_win) return g_list;
    if (this == g_list) return g_panel;
    return nullptr;
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(g_fontDummy);
}

bool InterfaceListControl2::GetColumnPos(int32_t column, int16_t* left, int16_t* right) {
    ev("GetColumnPos", {lab(this), column});
    *left = static_cast<int16_t>(ret("colL"));
    *right = static_cast<int16_t>(ret("colR"));
    return true;
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    int64_t len = 0;
    int64_t h = HashText(item->mText, &len);
    ev("AddItem", {lab(this), static_cast<int64_t>(item->mText - g_base), len, h, data, position, update});
    mItemCount++;
    return 0;
}

int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    ev("RemoveItem", {lab(this), position});
    mItemCount--;
    return 0;
}

void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), position}); }

int32_t InterfaceListControl2::FindString(char16_t* text, int32_t column) {
    int64_t len = 0;
    int64_t h = HashText(text, &len);
    ev("FindString", {lab(this), static_cast<int64_t>(text - g_base), len, h, column});
    return static_cast<int32_t>(ret("FindString"));
}

void InterfaceListControl2::SetData(int32_t position, int32_t data) { ev("SetData", {lab(this), position, data}); }

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

void FillContactList() { ev("FillContactList", {}); }
void FillBuddyList() { ev("FillBuddyList", {}); }
void UI_DoPostcard() { ev("UI_DoPostcard", {}); }

COnlineControl::COnlineControl() {}
void COnlineControl::EarmuffsOn() { ev("EarmuffsOn", {lab(this)}); }
void COnlineControl::EarmuffsOff() { ev("EarmuffsOff", {lab(this)}); }
void COnlineControl::VoteForThisPark() { ev("VoteForThisPark", {lab(this)}); }

namespace contacts_callee {
int32_t FontCharWidth(InterfaceFont*, char16_t character) {
    ev("CharWidth", {character});
    return static_cast<int32_t>(ret("CharWidth"));
}
void WindowSlot1c(InterfaceWindow* window, int32_t value) { ev("WindowSlot1c", {lab(window), value}); }
COnlineControl* NewOnlineControl() {
    void* p = tpw::ArenaAllocZeroed(0x100);
    int64_t l = newlab(p);
    ev("ctor", {l});
    return static_cast<COnlineControl*>(p);
}
}  // namespace contacts_callee

extern "C" {

uint64_t tpw_fx_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", &gContactsFontIndex},   {"busy", &gOnlineContactsBusy}, {"maxrows", &gContactsMaxRows},
        {"width", &gUIRealScreenWidth},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fx_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// the pointer globals (8 bytes on the host): which = 0 panel, 1 control
void tpw_fx_setptr(int32_t which, uint64_t value) {
    if (which == 0) gOnlinePanel = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(value));
    else gOnlineControl = reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(value));
}
uint64_t tpw_fx_getptr(int32_t which) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0 ? static_cast<void*>(gOnlinePanel) : static_cast<void*>(gOnlineControl)));
}
void tpw_fx_setobjs(uint64_t panel, uint64_t win, uint64_t list) {
    g_panel = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(panel));
    g_win = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(win));
    g_list = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(list));
}
void tpw_fx_setbase(uint64_t base) { g_base = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(base)); }

// the host offset of the list's item count (the Mac's is 0x150) and the size of the list class
int32_t tpw_fx_layout(int64_t* out) {
    alignas(16) static char buffer[sizeof(InterfaceListControl2)];
    InterfaceListControl2* list = reinterpret_cast<InterfaceListControl2*>(buffer);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&list->mItemCount) - buffer;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceListControl2));
    return n;
}

void tpw_fx_recv(uint64_t text, int32_t data) { OnlineReceiveString(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)), data); }
int32_t tpw_fx_handler(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return ContactListHandler(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b);
}
void tpw_fx_add(uint64_t text, int32_t data) { _OnlineAddBuddy(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)), data); }
void tpw_fx_remove(uint64_t text) { _OnlineRemoveBuddy(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text))); }
void tpw_fx_update(uint64_t text, int32_t data) { _OnlineUpdateBuddy(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)), data); }

}  // extern "C"
