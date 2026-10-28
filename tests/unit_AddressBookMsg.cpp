// Test library for the address book panel's message function and list filler (unit v5055; libtpw_unit_AddressBookMsg.dylib,
// tests/test_AddressBookMsg.py), main app:
//   src/game/ui/ui_Postcard.cpp   AddressBookMsg, FillAddressList
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The windows are
// fake objects: the edit box has the virtual SetText in the slot InterfaceWindow has it; the list control's members are fakes.
// The address book is a linked list of nodes (word 0 the next node, +0xc the text) the cursor fakes walk.
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "calllog.h"
#include "email.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSUserInfo* gUserInfo;
CFSOnlineSystem* gOnlineSystem;
InterfaceWindow* gAddressBookPanel;
char16_t gAddressRemoveName[0x40];

// The program's one game type object and its guard byte are TheGameType()'s storage (gLoansGameTypeStorage and
// gLoansGameTypeGuard, inline variables of FS_Online.h).

namespace {

uintptr_t gBookHead;              // the first node
AddressBookNode* gCursor;         // the cursor First and Next answer a pointer to
alignas(16) char gParent[0x140];
alignas(16) char gPanel[0x140];
alignas(16) char gParkInfo[0x100];
alignas(16) char16_t gString121[0x40];
alignas(16) char gListWindow[0x140];

struct EditWindow {
    virtual void v0() {}
    virtual void v1() {}
    virtual ~EditWindow() {}
    virtual int32_t v3() { return 0; }
    virtual void v4() {}
    virtual int16_t v5(int16_t) { return 0; }
    virtual int32_t v6(InterfaceWindow&) { return 0; }
    virtual int32_t v7(int32_t) { return 0; }
    virtual int32_t v8() { return 0; }
    virtual int32_t SetText(char16_t* text);
    char pad[0x140];
};
EditWindow gEditWindow;

int64_t KindOf(const void* p) {
    if (p == nullptr) return 0;
    return p == static_cast<const void*>(gAddressRemoveName) ? 1 : 2;
}

// The first eight code units of a wide string up to its end, then zeros (a NULL gives zeros).
void Codes16(const char16_t* t, int64_t* out) {
    bool live = t != nullptr;
    for (int i = 0; i < 8; i++) {
        int64_t c = live ? t[i] : 0;
        if (c == 0) live = false;
        out[i] = c;
    }
}

void Codes8(const char* t, int64_t* out) {
    bool live = t != nullptr;
    for (int i = 0; i < 8; i++) {
        int64_t c = live ? static_cast<unsigned char>(t[i]) : 0;
        if (c == 0) live = false;
        out[i] = c;
    }
}

}  // namespace

int32_t EditWindow::SetText(char16_t* text) {
    int64_t c[8];
    Codes16(text, c);
    ev("SetText", {lab(this), KindOf(text), c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
    return 1;
}

// ---- the fakes
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("Cancel", {static_cast<int32_t>(message)}); }
void RemoveAddressBook() { ev("RemoveAddressBook", {1}); }
void RemoveAddressBookEntry() { ev("RemoveAddressBookEntry", {1}); }

void AddToAddressField(char16_t* text) {
    int64_t c[8];
    Codes16(text, c);
    ev("AddToAddress", {KindOf(text), c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0x25) return reinterpret_cast<InterfaceWindow*>(&gEditWindow);
    if (id == 0x26) return reinterpret_cast<InterfaceWindow*>(gListWindow);
    return ret("GetWindow") != 0 ? reinterpret_cast<InterfaceWindow*>(gListWindow) : nullptr;
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    int64_t c[8];
    Codes16(item->mText, c);
    ev("AddItem", {lab(this), data, position, update, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
    return 0;
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

// The constructors are defined under their linker names so that no member initialiser runs (the members' own constructors are not
// in this library).
void FakeUserInfoCtor(CFSUserInfo* self) asm("__ZN11CFSUserInfoC1Ev");
void FakeUserInfoCtor(CFSUserInfo* self) { ev("CFSUserInfo", {newlab(self)}); }
void FakeOnlineCtor(CFSOnlineSystem* self) asm("__ZN15CFSOnlineSystemC1Ev");
void FakeOnlineCtor(CFSOnlineSystem* self) { ev("CFSOnlineSystem", {newlab(self)}); }

CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("ParkInfo", {lab(this)});
    return reinterpret_cast<CFSFTPParkInfo*>(gParkInfo);
}

CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(ret("GameType"));
}

AddressBookNode** AddressBook::First() {
    ev("First", {reinterpret_cast<char*>(this) == reinterpret_cast<char*>(&gUserInfo->mAddressBook) ? 1 : 0});
    gCursor = reinterpret_cast<AddressBookNode*>(gBookHead);
    return &gCursor;
}

AddressBookNode** AddressBook::Next() {
    ev("Next", {reinterpret_cast<char*>(this) == reinterpret_cast<char*>(&gUserInfo->mAddressBook) ? 1 : 0});
    AddressBookNode* next;   // the test's next link: a native pointer in the node's first 8 bytes (the Mac's first word)
    std::memcpy(&next, gCursor, sizeof(next));
    gCursor = next;
    return &gCursor;
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return gString121;
}

int swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    const char* text = va_arg(args, const char*);
    va_end(args);
    int64_t c[8];
    Codes8(text, c);
    ev("swprintf", {KindOf(buffer), static_cast<int64_t>(static_cast<int32_t>(size)), format[0], format[1], c[0], c[1], c[2], c[3],
                    c[4], c[5], c[6], c[7]});
    size_t i = 0;
    if (size != 0) {
        for (; i + 1 < size && text[i] != 0; i++) buffer[i] = static_cast<unsigned char>(text[i]);
        buffer[i] = 0;
    }
    return static_cast<int>(i);
}

uint32_t wcslen(const char16_t* text) {
    int64_t c[8];
    Codes16(text, c);
    ev("wcslen", {c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
    uint32_t n = 0;
    while (text[n] != 0) n++;
    return n;
}

extern "C" {

uint64_t tpw_ab_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},          {"gOnlineSystem", &gOnlineSystem},
        {"gPanelPtr", &gAddressBookPanel},  {"removeName", gAddressRemoveName},
        {"gtype", gLoansGameTypeStorage},   {"gguard", &gLoansGameTypeGuard},
        {"head", &gBookHead},               {"cursor", &gCursor},
        {"list", gListWindow},              {"edit", &gEditWindow},
        {"parent", gParent},                {"panel", gPanel},
        {"parkinfo", gParkInfo},            {"str121", gString121},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ab_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ab_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(AddressBookNode, mText));
    return n;
}

int32_t tpw_ab_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return AddressBookMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

void tpw_ab_fill(int32_t clear) { FillAddressList(clear); }

}  // extern "C"
