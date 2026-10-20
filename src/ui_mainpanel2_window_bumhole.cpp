// Unit v5143 (Mac file ui_mainpanel2.cpp): the bumhole window's message function, the game window toggle and SetOnOffText.
#include <cstdint>

#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

int32_t gGameWindowToggled;        // 0x10394984
InterfaceWindow* gSetOnOffRoot;    // 0x10394a14

// 0x101a886c (544 bytes).
int32_t bumhole(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10005) {
        if (a == 1) window->CaptureDevice();
    } else if (message < 0x10005 && message >= 0x10004) {
        if (a == 1) window->UnCaptureDevice();
    }
    Will_HandleCameraMessages(message, a, b);

    if (message == 0x1000a) {
        gKeyTableCamera->KeyDown(static_cast<int16_t>(a), static_cast<int16_t>((b >> 16) & 0xff));
    } else if (message < 0x1000a) {
        if (message == 0x10005) {
            int16_t shifts = CheckShifts();
            gKeyTableCamera->KeyDown(static_cast<int16_t>(a - 0x10), shifts);
        } else if (message < 0x10005 && message >= 0x10004) {
            int16_t shifts = CheckShifts();
            gKeyTableCamera->KeyUp(static_cast<int16_t>(a - 0x10), shifts);
        }
    } else if (message < 0x1000c) {
        int32_t mods = (b >> 16) & 0xff;
        gKeyTableCamera->KeyUp(static_cast<int16_t>(a), static_cast<int16_t>(mods));
        if (gGameWorld->mF1DA738 == 4) {
            if (a == 0x1b && mods == 0) CreateMainMenu(0);
        } else {
            int16_t found = static_cast<int16_t>(gKeyTableGame->Search(static_cast<int16_t>(a), static_cast<int16_t>(mods)));
            if (found == 4 || (a == 0x1b && mods == 0)) {
                ToggleGameWindow();
            } else {
                found = static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), static_cast<int16_t>(mods)));
                if (found == 0xf) UI_DoPostcard();
            }
        }
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a8ac0 (308 bytes).
void ToggleGameWindow() {
    if (gGameWindow == nullptr) return;
    if (gGameWindowToggled == 0) {
        if ((gFirstPersonFlags & 0x16) != 0) return;
        if (gBumholeWindow == nullptr) {
            gBumholeWindow = gUIRootWindow->CreateObject(1, 1, 0xddf22, 0, 0, 0x7ff, 0x5ff);
            if (gBumholeWindow != nullptr) {
                gBumholeWindow->SetCallbackFunction(bumhole);
                gBumholeWindow->CaptureKb(0);
            }
        }
    } else {
        if (gBumholeWindow != nullptr) delete gBumholeWindow;
        gBumholeWindow = nullptr;
    }
    gGameWindow->Show(static_cast<int16_t>(gGameWindowToggled));
    gGameWindowToggled = 1 - gGameWindowToggled;
    if (gGameWindowToggled == 0) UIMain_DecideKbCapture();
}

// 0x101a8e60 (304 bytes).
void SetOnOffText(int32_t window, int32_t text, bool first, bool second, bool button, int32_t buttonId) {
    InterfaceWindow* parent = gSetOnOffRoot->GetWindow(window);
    InterfaceWindow* label = parent->GetWindow(0x1d4c9);
    int32_t id;
    if (second) {
        id = first ? 0x14f : 0x14e;
    } else {
        id = first ? 0x14d : 0x14c;
    }
    char16_t local[38];
    wcsncpy(local, UIMain_GetString(id), 0x1f);
    UIMain_GetString(text);
    wcscat(gUIMainConvertBuffer, local);
    label->SetText(gUIMainConvertBuffer);
    if (button) {
        InterfaceButton* control = static_cast<InterfaceButton*>(gSetOnOffRoot->GetWindow(buttonId));
        if (first) {
            control->SetState(0, 1);
        } else {
            control->SetState(1, 1);
        }
    }
}
