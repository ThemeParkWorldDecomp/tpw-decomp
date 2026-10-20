// ui_main.cpp, main app 0x1017fdf8 and 0x101867f8.
#include "ui_main.h"
#include "HLSprite.h"
#include "TPSound.h"
#include "TPSettings.h"
#include "ui_controls.h"
#include "ui_ime.h"
#include "ui_ErrorDisplay.h"

int32_t EffectPeekMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t) {
    CoolRect rect;
    if (message == 0x100)
        SOUND_SendEvent(0, gSoundBanks[3], (window->GetAttr() & 0x10) != 0 ? 0xbd : 0x1f, 0, 0, 0);
    if (message == 0x14 || message == 0x20) {
        if (window->WhatAmI() == 5) gUIIME.DeActivate();
    } else if (message == 0x1f) {
        if (window->WhatAmI() == 5) gUIIME.Activate(window);
    } else if (message == 0x801) {
        gUIIME.InvalidateStoredString(window->GetWindow(a));
    }
    if (gPlayerSettings.mCurrent.mTooltipsOn != 0) {
        if (message == 0x14) {
            if (window->WhatAmI() == 2) {
                auto* button = static_cast<InterfaceButton*>(window);
                if (button->mHilight == 1 || button->GetState() == 1) BTN_HighLight_Kill();
            }
        } else if (message == 0x11) {
            if (window->WhatAmI() == 2 && static_cast<InterfaceButton*>(window)->mHilight == 1) BTN_HighLight_Kill();
        } else if (message == 0x13 || message == 0x10001) {
            if (window->WhatAmI() == 2 &&
                (message == 0x10001 || static_cast<InterfaceButton*>(window)->GetState() == 1)) {
                window->GetPosition(&rect, 1);
                const int32_t width = rect.mRight - rect.mLeft;
                const int32_t height = rect.mBottom - rect.mTop;
                // Signed division truncates toward zero, matching srawi followed by addze.
                const float x = static_cast<float>(rect.mLeft + width / 2 - 1024) * (1.0f / 1024.0f);
                const float y = static_cast<float>(rect.mTop + height / 2 - 768) / 768.0f;
                if (message == 0x10001)
                    BTN_HighLight_On(x, y, static_cast<float>(width) / 2048.0f, static_cast<float>(height) / 1536.0f);
                else
                    BTN_HighLight_Move(x, y);
            }
        } else if (message == 0x10002 && window->WhatAmI() == 2) {
            BTN_HighLight_Off();
        }
    }
    return 0;
}

void UnPauseUI() {
    if (gUIRootWindow != nullptr) {
        UI_UnPauseTimerSystem();
        HidePauseWindow();
    }
}
