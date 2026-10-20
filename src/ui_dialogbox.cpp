// ui_ErrorDisplay.cpp (main app), unit w43: UI_DialogBox 0x101f1800, 1008 bytes.
#include "ui_ErrorDisplay.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

uint8_t gUIMainKeyTablesDisabled;   // 0x1038cc54

// 0x101823c8, 12 bytes: the bookkeeping byte, independently of the tables' enabled words.
uint8_t UIMain_IsKeyTableDisabled() { return gUIMainKeyTablesDisabled; }

// 0x10182364, 48 bytes: first disable the six tables, then set the bookkeeping byte.
void UIMain_DisableAllKeyTables() {
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
}

// 0x101d8b64, 16 bytes.
void UI_PauseTimerSystem() { gUITimersPaused = 1; }

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                            dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag) {
    InterfaceWindow* window = Dialog(gDialogBoxWindowData, DialogBoxCallback, 1);
    if (window == nullptr) return nullptr;
    CoolRect unused;
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    window->SetFont(GetFontTableEntry(static_cast<uint32_t>(params.mValue8)));
    window->SetFontColour(static_cast<int8_t>(params.mRed), static_cast<int8_t>(params.mGreen),
                          static_cast<int8_t>(params.mBlue), 0xff);
    window->SetClientDrawText(const_cast<char16_t*>(params.mText));
    const dlgbutton buttons[3] = {button1, button2, button3};
    const dlgcallback callbacks[3] = {callback1, callback2, callback3};
    for (int16_t index = 0; index < 3; ++index) {
        InterfaceWindow* button = window->GetWindow(0x9873c8 + index);
        if (buttons[index] == dlgbutton(0)) {
            button->Show(0);
        } else {
            InterfaceDrawObject* draw = nullptr;
            if (buttons[index] == dlgbutton(1)) draw = DrawObjectLibrary::FindDrawObject(const_cast<char*>("b_okay"));
            else if (buttons[index] == dlgbutton(2)) draw = DrawObjectLibrary::FindDrawObject(const_cast<char*>("b_exit"));
            button->SetDrawObject(draw);
            button->Show(1);
            window->setExtraParam(index, reinterpret_cast<void*>(callbacks[index]));
        }
    }
    window->setExtraParam(3, reinterpret_cast<void*>(static_cast<intptr_t>(params.mValueC)));
    if (flag) {
        window->setExtraParam(4, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
    } else {
        window->setExtraParam(4, reinterpret_cast<void*>(static_cast<uintptr_t>(UIMain_IsKeyTableDisabled())));
        UIMain_DisableAllKeyTables();
    }
    window->setExtraParam(5, nullptr);
    if (gGame.mF3C == 1 && gLobby == nullptr) {
        gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
        gGame.mF3C = 0;
        UI_PauseTimerSystem();
        window->setExtraParam(5, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
    }
    return window;
}
