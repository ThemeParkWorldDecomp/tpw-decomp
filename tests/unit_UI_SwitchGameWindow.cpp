// Test library for UI_SwitchGameWindow and UI_SetGameWindowCursor (unit v5313; libtpw_unit_UI_SwitchGameWindow.dylib).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
CWorld* gGameWorld;

// TheGameType's object and its guard byte are gLoansGameTypeStorage and gLoansGameTypeGuard (FS_Online.h).

// ---- the fakes
bool CWorld::IsPlayingWinningAnim() {
    ev("IsPlayingWinningAnim", {});
    return ret("IsPlayingWinningAnim") != 0;
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }

CGameType::CGameType() { ev("CGameType", {lab(this)}); }

void ChatGoFirstPerson() { ev("ChatGoFirstPerson", {}); }
void ChatBackToThirdPerson() { ev("ChatBackToThirdPerson", {}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }

extern "C" {

uint64_t tpw_usw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameWindow", &gGameWindow},
        {"gFirstPersonWindow", &gFirstPersonWindow},
        {"gGameWorld", &gGameWorld},
        {"gGameWindowCursor", &gGameWindowCursor},
        {"gameType", gLoansGameTypeStorage},
        {"gameTypeGuard", &gLoansGameTypeGuard},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_usw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_usw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    return n;
}

void tpw_usw_switch(int32_t first) { UI_SwitchGameWindow(first); }
void tpw_usw_cursor(int32_t cursor) { UI_SetGameWindowCursor(static_cast<int16_t>(cursor)); }

}  // extern "C"
