// InitialiseMainMenu 0x101fb760 (1348) and InitialiseLobbyMenu 0x101fbcd0 (864): make the main menu object gMainMenu and its items.
#define TPW_UIMENU_CTOR_EXTERNAL 1
#include <cstdint>

#include "FS_Online.h"
#include "World.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_menu.h"

namespace {

// The online system is made when it is not there yet (the original repeats this test before each use).
CFSOnlineSystem* Online() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    return gOnlineSystem;
}

int32_t Add(int32_t position, int32_t stringId, int32_t id, int32_t colour) {
    return gMainMenu->AddItem(position, UIMain_GetString(stringId), id, 0, colour, colour, colour);
}

}  // namespace

void InitialiseMainMenu() {
    gMainMenu = new UIMenu();
    gMainMenu->Init(MainGameMenuCB, MainMenuItemCB);
    const int32_t c = 0x80;
    CGameType& type = TheGameType();   // the guard byte test and the constructor on first use
    if (type.mType == 1) {
        int32_t p = Add(10, 7, 0x10, c);
        p = Add(p + 5, 0xc, 0x13, c);
        Add(p + 5, 8, 0x14, c);
    } else if (gGameWorld->mF1DA738 == 4) {
        int32_t p = Add(10, 3, 0x16, c);
        p = Add(p + 5, 10, 0x17, c);
        p = Add(p + 5, 7, 0x15, c);
        p = Add(p + 5, 0xc, 0x18, c);
        Add(p + 5, 8, 0x19, c);
    } else {
        int32_t n = 5;
        if (Online()->AreWeLoggedIn() != 0) n = 0;
        int32_t p = Add(n << 1, 3, 1, c);
        p = Add(p + n, 4, 2, c);
        p = Add(p + n, 10, 3, c);
        p = Add(p + n, 5, 4, c);
        if (Online()->AreWeLoggedIn() != 0) p = Add(p + n, 2, 7, c);
        if (Online()->AreWeProcessing() == 0) p = Add(p + n, 6, 5, c);
        p = Add(p + n, 7, 0, c);
        p = Add(p + n, 0xc, 6, c);
        Add(p + n, 8, 8, c);
    }
}

void InitialiseLobbyMenu() {
    gMainMenu = new UIMenu();
    gMainMenu->Init(MainLobbyMenuCB, MainMenuItemCB);
    int32_t p = 0;
    if (Online()->mF108 != 0) {
        p = Add(p + 5, 0xe0, 0xd, 0);
        p = Add(p + 5, 7, 0xe, 0);
        Add(p + 5, 8, 0xf, 0);
    } else {
        if (Online()->AreWeLoggedIn() == 0) p = Add(p + 5, 1, 9, 0);
        else p = Add(p + 5, 2, 0xa, 0);
        if (Online()->AreWeProcessing() == 0) p = Add(p + 5, 6, 0xb, 0);
        if (gLobbyScreens->mThemeIslands->IsVisible() != 0) p = Add(p + 5, 0xd, 0xc, 0);
        p = Add(p + 5, 7, 0xe, 0);
        Add(p + 5, 8, 0xf, 0);
    }
}
