// Test library for CreateMainMenu and HideMainMenu (unit v5303; libtpw_unit_HideMainMenu.dylib, tests/test_HideMainMenu.py), main app:
//   src/game/ui/ui_mainmenu.cpp   CreateMainMenu, HideMainMenu, gMainMenuPauseFlag
//   src/game/ui/UIMenu.cpp        UIMenuDestruct (UIMenu::~UIMenu), the menu's operator delete
//
// The other callees are recording fakes (the menus' makers set gMainMenu to the menu the test allocated; UI_SendMessage records what the
// real destructor tells the menu's window). gMainMenu and gGame are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Game.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_menu.h"

using namespace calllog;

UIMenu* gMainMenu;
// gGame without its constructor (the unit does not link Game.cpp): raw storage under the program's symbol.
alignas(16) char gGameStorage[sizeof(CGame)] __asm__("_gGame");

namespace {
UIMenu* gTestMenu;
}

void InitialiseLobbyMenu() {
    ev("InitialiseLobbyMenu", {});
    gMainMenu = gTestMenu;
}
void InitialiseMainMenu() {
    ev("InitialiseMainMenu", {});
    gMainMenu = gTestMenu;
}
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void CGame::Pause(ePauseFlag flag, bool allSounds) { ev("Pause", {lab(this), static_cast<int32_t>(flag), allSounds ? 1 : 0}); }
void CGame::Unpause() { ev("Unpause", {lab(this)}); }
void UI_PauseTimerSystem() { ev("UI_PauseTimerSystem", {}); }
void UI_UnPauseTimerSystem() { ev("UI_UnPauseTimerSystem", {}); }
void UIMenu::Show(int32_t a) { ev("Show", {lab(this), a}); }
void UIMenu::Hide() { ev("Hide", {lab(this)}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

extern "C" {

uint64_t tpw_hm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGame", gGameStorage},
        {"flag", &gMainMenuPauseFlag},
        {"menu", &gMainMenu},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A menu made with new (HideMainMenu deletes it); the makers' fakes install it.
uint64_t tpw_hm_newmenu() {
    gTestMenu = new UIMenu();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTestMenu));
}

// The menu's window word (a label of the test's, the window is never touched).
void tpw_hm_setwindow(uint64_t window) { gTestMenu->mWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)); }

int32_t tpw_hm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CGame, mF3C));
    return n;
}

void tpw_hm_create(int32_t a) { CreateMainMenu(a); }
void tpw_hm_hide() { HideMainMenu(); }

}  // extern "C"
