// UI_SwitchGameWindow 0x101a8cd8 (240 bytes) and UI_SetGameWindowCursor 0x101a8c58 (80 bytes).
#include <cstdint>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"

int32_t gGameWindowCursor;   // 0x103949b0

// 0x101a8c58: when there is a game window, sets its cursor and remembers it (sign-extended from 16 bits).
void UI_SetGameWindowCursor(int16_t cursor) {
    if (gGameWindow != nullptr) {
        gGameWindow->setCursor(cursor);
        gGameWindowCursor = cursor;
    }
}

void UI_SwitchGameWindow(int32_t first) {
    if (static_cast<int32_t>(gGameWorld->mF1DA738) == 4) return;
    if (gGameWorld->IsPlayingWinningAnim()) return;
    UI_SendMessage(gGameWindow, 6, first, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 1 - first, 0);
    if (TheGameType().mType == kGameTypeOnline) {
        if (first == 0) {
            ChatGoFirstPerson();
        } else {
            ChatBackToThirdPerson();
        }
    }
    UIMain_DecideKbCapture();
}
