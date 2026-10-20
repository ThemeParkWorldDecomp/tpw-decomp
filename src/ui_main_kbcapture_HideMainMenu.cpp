// Mac file ui_mainmenu.cpp: CreateMainMenu 0x101fc05c (188) and HideMainMenu 0x101fc140 (136).
#include <cstdint>

#include "Game.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_menu.h"

uint8_t gMainMenuPauseFlag;   // 0x10396a54

// Stand-in for UIMenu::~UIMenu 0x1018ac8c (not translated yet): frees the menu. Weak so a translation or a test fake replaces it.
__attribute__((weak)) void DestroyUIMenu(UIMenu* menu) { delete menu; }

// Makes the main menu (the lobby's when `a` is not 0) and shows it; nothing when it already exists.
void CreateMainMenu(int32_t a) {
    if (gMainMenu != nullptr) return;
    if (a != 0) {
        InitialiseLobbyMenu();
        gMainMenuPauseFlag = 1;
    } else {
        InitialiseMainMenu();
        RemoveSlideoutPanel(1);
        if (gGame.mF3C == 1) {
            gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
            gGame.mF3C = 0;
            gMainMenuPauseFlag = 0;
            UI_PauseTimerSystem();
        } else {
            gMainMenuPauseFlag = 1;
        }
    }
    gMainMenu->Show(a);
}

// Unpauses the game when the menu had paused it, then hides and deletes the menu (no NULL check, as the original).
void HideMainMenu() {
    if (gGame.mF3C == 0 && gMainMenuPauseFlag == 0) {
        gGame.mF3C = 1;
        gGame.Unpause();
        UI_UnPauseTimerSystem();
    }
    gMainMenu->Hide();
    DestroyUIMenu(gMainMenu);
    gMainMenu = nullptr;
}
