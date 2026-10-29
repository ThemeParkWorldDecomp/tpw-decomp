// Test library for NewsPanelMsg and RemoveOnlineLoginPanel (unit v5286).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

CLBLobby* gLobby = nullptr;
InterfaceWindow* gOnlineNewsPanel = nullptr;
int32_t gOnlineLoginPanelFlag = 0;
extern void (*gOnlineLoginFailureFn)();
int32_t NewsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

static int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {Raw(window), message, a, b});
    return static_cast<int32_t>(ret("UI_SendMessage"));
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
void RemoveOnlineNewsPanel() { ev("RemoveOnlineNewsPanel", {}); }
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultDialogCallback", {Raw(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultDialogCallback"));
}
static void HookFake() { ev("hook", {}); }

extern "C" {

uint64_t tpw_nm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLobby", &gLobby},
        {"news", &gOnlineNewsPanel},
        {"flag", &gOnlineLoginPanelFlag},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_nm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_nm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mWindow));
    return n;
}

void tpw_nm_sethook(int32_t on) { gOnlineLoginFailureFn = on ? HookFake : nullptr; }
// the pointers in the globals are 64-bit on the host: the test hands over 32-bit values
void tpw_nm_setptrs(uint32_t window, uint32_t news) {
    alignas(16) static char lobbyBlock[512];
    gLobby = reinterpret_cast<CLBLobby*>(lobbyBlock);
    gLobby->mWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
    gOnlineNewsPanel = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(news));
}
uint32_t tpw_nm_getnews() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gOnlineNewsPanel)); }

uint32_t tpw_nm_news(uint32_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(NewsPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
void tpw_nm_login(int32_t arg) { RemoveOnlineLoginPanel(arg); }

}  // extern "C"
