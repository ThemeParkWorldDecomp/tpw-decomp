// Test library for the online outbox panel (unit v5239; libtpw_unit_OutboxPanelMsg.dylib, tests/test_OutboxPanelMsg.py), main app:
//   src/UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg_OutboxPanelMsg.cpp   OutboxPanelMsg, SendAndDeleteOutbox
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues. The
// globals of other files are defined here, zero at start like the Mac's bss. The objects the code reads (the user info, the list, the
// lobby) are blocks the test fills through wrappers, because the host layouts differ from the Mac's.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "calllog.h"
#include "email.h"
#include "lobby.h"
#include "platform/arena.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSUserInfo* gUserInfo;                   // 0x10380b34
CFSOnlineSystem* gOnlineSystem;           // 0x10380ab0
CLBLobby* gLobby;                         // 0x102e4888
OnlineConfig gOnlineConfig;               // 0x102b2b70
InterfaceWindow* gOutboxWindow;           // 0x10395e3c
uint32_t gOutboxPreview[9];               // 0x10395e10
InterfaceWindow* gOutboxSendingWindow;    // 0x10395e0c
int32_t gMailStatusShownB;                // 0x10395e00
int32_t gMailStatusShownA;                // 0x10395e08
int32_t gMailStatusB;                     // 0x10395dfc
int32_t gMailStatusA;                     // 0x10395e04

OnlineConfig::OnlineConfig() {}

namespace {

bool gQuiet = false;

enum { kBlockUser, kBlockOnline, kBlockList, kBlockLobby, kBlockWin, kBlockOwin, kBlockLwin, kBlockChild, kBlockSendwin, kBlocks };
const char* const kBlockNames[kBlocks] = {"user", "online", "list", "lobby", "win", "owin", "lwin", "child", "sendwin"};
void* gBlocks[kBlocks];

void* Block(int index) {
    if (gBlocks[index] == nullptr) {
        gBlocks[index] = tpw::ArenaAllocZeroed(0x2000);   // the user info block is raw zero memory too (see the constructor fake)
    }
    return gBlocks[index];
}

// A queue token as a pointer: 0 is NULL, 1 is the list, 2 is the child window, 3 is the sending window, else the raw number.
InterfaceWindow* TokenWindow(int64_t token) {
    if (token == 1) return static_cast<InterfaceWindow*>(Block(kBlockList));
    if (token == 2) return static_cast<InterfaceWindow*>(Block(kBlockChild));
    if (token == 3) return static_cast<InterfaceWindow*>(Block(kBlockSendwin));
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(token));
}

uint32_t Low32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes
// CFSUserInfo's constructor, defined under its mangled name: a normal definition would need the constructors of its members (strings,
// lists), which other files define. The fake clears the object (the Mac's memory is zero here too) and records the event.
extern "C" void tpw_fake_userinfo_ctor(CFSUserInfo* self) __asm__("__ZN11CFSUserInfoC1Ev");
extern "C" void tpw_fake_userinfo_ctor(CFSUserInfo* self) {
    std::memset(static_cast<void*>(self), 0, sizeof(CFSUserInfo));
    if (!gQuiet) ev("CFSUserInfo", {newlab(self)});
}

CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }

int32_t CFSOnlineSystem::CanWeSendEmail() {
    ev("CanWeSendEmail", {lab(this)});
    return static_cast<int32_t>(ret("CanWeSendEmail"));
}

void CFSOnlineSystem::LoggedInGetNews() { ev("LoggedInGetNews", {lab(this)}); }

void Outbox::Delete() { ev("OutboxDelete", {lab(this)}); }

int32_t Outbox::Send(const char* server, int16_t port, char* extra) {
    ev("OutboxSend", {lab(this), port, lab(extra)}, {server});
    return static_cast<int32_t>(ret("OutboxSend"));
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void TbRenderArea::FreeMemory() { ev("FreeMemory", {lab(this)}); }

void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return TokenWindow(ret("GetWindow", id, 0));
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData", position, 0));
    return 1;
}

void InterfaceListControl2::SetData(int32_t position, int32_t data) { ev("SetData", {lab(this), position, data}); }

void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {lab(this), position, item[2].mInt, item[3].mInt, columns, update, unused});
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

void SetCurrentEmailInfo(int32_t number) { ev("SetCurrentEmailInfo", {number}); }

void CalcEmailAddressScrollBar() { ev("CalcEmailAddressScrollBar", {}); }

void UpdatePreview() { ev("UpdatePreview", {}); }

void ColumnClick(int32_t column) { ev("ColumnClick", {column}); }

int32_t MarkOutboxEmail(int32_t flags) {
    ev("MarkOutboxEmail", {flags});
    return static_cast<int32_t>(ret("MarkOutboxEmail"));
}

void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }

int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultDialogCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultDialogCallback"));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("UIMain_GetString") & 0xffffffffu));
}

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("UI_DialogBox", {Low32(params.mText), params.mRed | (params.mGreen << 8) | (params.mBlue << 16), params.mValue8, params.mValueC, button1,
                        Low32(reinterpret_cast<void*>(callback1)), button2, Low32(reinterpret_cast<void*>(callback2)), button3,
                        Low32(reinterpret_cast<void*>(callback3)), flag});
    return TokenWindow(ret("UI_DialogBox"));
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == OutboxSendingMsg});
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}


// Never called: its address is what the sending dialog gets as its callback.
int32_t OutboxSendingMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

// The host address of a global or block by the module's name for it.
uint64_t tpw_ob_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},
        {"gOnlineSystem", &gOnlineSystem},
        {"gLobby", &gLobby},
        {"gOutboxWindow", &gOutboxWindow},
        {"gOutboxSendingWindow", &gOutboxSendingWindow},
        {"gOutboxPreview", gOutboxPreview},
        {"gMailStatusShownB", &gMailStatusShownB},
        {"gMailStatusShownA", &gMailStatusShownA},
        {"gMailStatusB", &gMailStatusB},
        {"gMailStatusA", &gMailStatusA},
        {"outbox", &static_cast<CFSUserInfo*>(Block(kBlockUser))->mOutbox},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    for (int i = 0; i < kBlocks; ++i)
        if (std::strcmp(kBlockNames[i], name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Block(i)));
    return 0;
}

// A pointer global is 64 bits on the host: the test writes and reads it through these.
void tpw_ob_setptr(const char* name, uint64_t value) {
    *reinterpret_cast<uintptr_t*>(tpw_ob_addr(name)) = static_cast<uintptr_t>(value);
}

uint64_t tpw_ob_getptr(const char* name) { return *reinterpret_cast<uintptr_t*>(tpw_ob_addr(name)); }

// The case's objects: the user info's outbox (the list the Mac keeps at 0x8c has `size` entries, the word at 0x58 is cleared), the
// list's entry count, the lobby's window (the block "lwin").
void tpw_ob_world(uint32_t size, int32_t count) {
    CFSUserInfo* user = static_cast<CFSUserInfo*>(Block(kBlockUser));
    // The list is zero memory, never constructed: only its size word is read (size()), so the word is found in a real list and set.
    std::list<uint32_t> probe(1);
    const size_t* probeWords = reinterpret_cast<const size_t*>(&probe);
    size_t at = 0;
    while (probeWords[at] != 1) ++at;
    reinterpret_cast<size_t*>(&user->mOutbox.mF28)[at] = size;
    user->mOutbox.mF58 = 0;
    static_cast<InterfaceListControl2*>(Block(kBlockList))->mItemCount = count;
    static_cast<CLBLobby*>(Block(kBlockLobby))->mWindow = static_cast<InterfaceWindow*>(Block(kBlockLwin));
}

// The word at 0x58 of the outbox of the user info at `user`: 1 when it holds the callback vector's address.
uint32_t tpw_ob_callback_set(uint64_t user) {
    CFSUserInfo* info = reinterpret_cast<CFSUserInfo*>(static_cast<uintptr_t>(user));
    if (info == nullptr) return 0xffffffffu;
    uint32_t word = static_cast<uint32_t>(info->mOutbox.mF58);
    if (word == 0) return 0;
    return word == Low32(gOutboxCallbackVector) ? 1 : word;
}

uint32_t tpw_ob_list_size(uint64_t user) {
    CFSUserInfo* info = reinterpret_cast<CFSUserInfo*>(static_cast<uintptr_t>(user));
    return info == nullptr ? 0xffffffffu : static_cast<uint32_t>(info->mOutbox.mF28.size());
}

void tpw_ob_server(const char* text, int32_t port) {
    gOnlineConfig.mMailServer = text;
    gOnlineConfig.mMailPort = static_cast<int16_t>(port);
}

void tpw_ob_send(int32_t send) { SendAndDeleteOutbox(send != 0); }

int32_t tpw_ob_panel(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OutboxPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
