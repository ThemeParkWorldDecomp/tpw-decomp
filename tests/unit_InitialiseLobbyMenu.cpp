// Test library for InitialiseMainMenu and InitialiseLobbyMenu (unit v5230; libtpw_unit_InitialiseLobbyMenu.dylib,
// tests/test_InitialiseLobbyMenu.py), main app:
//   src/ui_main_kbcapture_InitialiseLobbyMenu.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: the menu's
// constructor, Init and AddItem, UIMain_GetString, the online system's constructor and its two questions, the game type's constructor and
// the screen's IsVisible. The globals of other files the unit uses are defined here.
#define TPW_UIMENU_CTOR_EXTERNAL 1
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "World.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_menu.h"

using namespace calllog;

// ---- globals of other files, defined here
UIMenu* gMainMenu;
CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
CWorld* gGameWorld;

namespace {

// A stand-in for the theme islands screen: the same vtable slots up to IsVisible (two destructors, eight virtuals).
struct FakeScreen {
    virtual ~FakeScreen() {}
    virtual void V2() {}
    virtual void V3() {}
    virtual void V4() {}
    virtual void V5() {}
    virtual void V6() {}
    virtual void V7() {}
    virtual void V8() {}
    virtual void V9() {}
    virtual int32_t IsVisible() {
        ev("IsVisible", {lab(this)});
        return static_cast<int32_t>(ret("IsVisible"));
    }
};

FakeScreen gFakeScreen;

int64_t Addr(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes
UIMenu::UIMenu() { ev("UIMenu", {newlab(this)}); }

void UIMenu::Init(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t),
                  int32_t (*itemHandler)(InterfaceWindow*, int32_t, int32_t, int32_t)) {
    ev("Init", {lab(this), lab(reinterpret_cast<const void*>(handler)), lab(reinterpret_cast<const void*>(itemHandler))});
}

int32_t UIMenu::AddItem(int32_t position, const char16_t* text, int32_t id, int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("AddItem", {lab(this), position, static_cast<int64_t>(static_cast<uint32_t>(Addr(text))), id, a, b, c, d});
    return static_cast<int32_t>(ret("AddItem"));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x7000 + id));
}

CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem", {newlab(this)});
    mF108 = static_cast<int32_t>(ret("f108"));
}

int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}

int32_t CFSOnlineSystem::AreWeProcessing() {
    ev("AreWeProcessing", {lab(this)});
    return static_cast<int32_t>(ret("AreWeProcessing"));
}

CGameType::CGameType() {
    ev("CGameType", {});
    mType = static_cast<GameType>(ret("gametype"));
}

int32_t MainLobbyMenuCB(InterfaceWindow*, int32_t, int32_t, int32_t) { return 1; }
int32_t MainGameMenuCB(InterfaceWindow*, int32_t, int32_t, int32_t) { return 2; }
int32_t MainMenuItemCB(InterfaceWindow*, int32_t, int32_t, int32_t) { return 3; }

extern "C" {

uint64_t tpw_ilm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"menu", &gMainMenu},
        {"online", &gOnlineSystem},
        {"screens", &gLobbyScreens},
        {"world", &gGameWorld},
        {"cb_lobby", reinterpret_cast<const void*>(&MainLobbyMenuCB)},
        {"cb_game", reinterpret_cast<const void*>(&MainGameMenuCB)},
        {"cb_item", reinterpret_cast<const void*>(&MainMenuItemCB)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ilm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ilm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mF108));
    return n;
}

// A screen system whose theme islands screen answers IsVisible from the queue; answers its address.
uint64_t tpw_ilm_make_screens() {
    CLBScreenSystem* system = static_cast<CLBScreenSystem*>(tpw::ArenaAllocZeroed(sizeof(CLBScreenSystem)));
    system->mThemeIslands = reinterpret_cast<CLBScreenComponentThemeIslands*>(&gFakeScreen);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(system));
}

void tpw_ilm_settype(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_ilm_gettype() { return static_cast<int32_t>(TheGameType().mType); }

void tpw_ilm_main() { InitialiseMainMenu(); }
void tpw_ilm_lobby() { InitialiseLobbyMenu(); }

}  // extern "C"
