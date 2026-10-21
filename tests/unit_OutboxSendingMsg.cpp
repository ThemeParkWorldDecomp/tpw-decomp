// Test library for the outbox window's messages (unit v5031; libtpw_unit_OutboxSendingMsg.dylib, tests/test_OutboxSendingMsg.py), main app:
//   src/game/ui/ui_onlineOutbox.cpp   MarkOutboxEmail, SendMailStatusDisplay, OutboxSendingMsg
//
// The callees outside the unit (the user info constructor, the outbox cursor and marks, GetWindow, Show, UI_PostMessage,
// DialogBoxCallback) are recording fakes that answer from the test's queues. gUserInfo and gLobby are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;

CFSUserInfo* gUserInfo;
CLBLobby* gLobby;

namespace {
// 1 when `p` is the outbox inside the user info (Mac: gUserInfo + 0x64)
int64_t IsOutbox(const Outbox* p) { return gUserInfo != nullptr && p == &gUserInfo->mOutbox ? 1 : 0; }
}  // namespace

// ---- the fakes

// The user info constructor is faked at its symbol, so that its member constructors (strings, lists) are not pulled in.
extern "C" void tpw_osm_userctor(void* self) {
    std::memset(self, 0, sizeof(CFSUserInfo));
    ev("CFSUserInfo", {});
}
__asm__(
    ".globl __ZN11CFSUserInfoC1Ev\n"
    "__ZN11CFSUserInfoC1Ev:\n"
    "  b _tpw_osm_userctor\n"
    ".globl __ZN11CFSUserInfoC2Ev\n"
    "__ZN11CFSUserInfoC2Ev:\n"
    "  b _tpw_osm_userctor\n");

void Outbox::First() { ev("First", {IsOutbox(this)}); }
bool Outbox::Next() {
    ev("Next", {IsOutbox(this)});
    return ret("Next") != 0;
}
void Outbox::MarkToSend() { ev("MarkToSend", {IsOutbox(this)}); }
void Outbox::UnmarkToSend() { ev("UnmarkToSend", {IsOutbox(this)}); }
void Outbox::MarkToDelete() { ev("MarkToDelete", {IsOutbox(this)}); }
void Outbox::UnmarkToDelete() { ev("UnmarkToDelete", {IsOutbox(this)}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return this;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return static_cast<int32_t>(ret("Show"));
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_PostMessage"));
}

int32_t DialogBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DialogBoxCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DialogBoxCallback"));
}

extern "C" {

uint64_t tpw_osm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"window", &gOutboxSendingWindow}, {"shownB", &gMailStatusShownB}, {"shownA", &gMailStatusShownA},
        {"statusB", &gMailStatusB},        {"statusA", &gMailStatusA},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_osm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_osm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 4;
    return n;
}

// The lobby, with `window` as its window (Mac +4).
void tpw_osm_setlobby(uint64_t window) {
    gLobby = static_cast<CLBLobby*>(tpw::ArenaAllocZeroed(sizeof(CLBLobby)));
    gLobby->mWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
}

// The user info: none when `present` is 0, else a block whose outbox mail list (Mac word 0x8c) holds `count`.
void tpw_osm_setuser(int32_t present, uint32_t count) {
    gUserInfo = nullptr;
    if (!present) return;
    gUserInfo = static_cast<CFSUserInfo*>(tpw::ArenaAllocZeroed(sizeof(CFSUserInfo)));
    // libc++'s list keeps its size after the two links
    *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(&gUserInfo->mOutbox.mF28) + 2 * sizeof(void*)) = count;
}

// 0 when there is no user info, else 1
int32_t tpw_osm_userstate() { return gUserInfo != nullptr ? 1 : 0; }

int32_t tpw_osm_mark(int32_t flags) { return MarkOutboxEmail(flags); }
void tpw_osm_status() { SendMailStatusDisplay(); }
int32_t tpw_osm_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OutboxSendingMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
