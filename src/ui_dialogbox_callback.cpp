// The dialog box's message functions (main app, SimTheme Park 2.04b3; the inventory files them under ui_ErrorDisplay.cpp), and the two
// small functions they call that undo UI_DialogBox's pause and key table switch-off. Unit w47: DialogBoxCallback 0x101f1520 (676 bytes),
// DefaultDialogCallback 0x101f1448 (152), UIMain_EnableAllKeyTables 0x10182304 (48) and UI_UnPauseTimerSystem 0x101d8ba0 (96; the
// inventory files it under ui_cityLobbyController.cpp).
//
// UI_DialogBox (src/ui_dialogbox.cpp) keeps in the dialog's extra parameters 0 to 2 the buttons' callbacks (NULL for none), 3 their
// argument, 4 the key table bookkeeping byte it found (-1: it left the tables alone) and 5 whether it paused the game.
#include "Game.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

namespace {

constexpr int32_t kMessageClose = 4;
constexpr int32_t kMessageDestroy = 0x14;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kFirstButton = 0x9873c8;   // the three button windows UI_DialogBox makes have these ids, in order

// The extra parameters are the Mac's 32-bit words.
int32_t ExtraWord(InterfaceWindow* window, int16_t index) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(index)));
}

}  // namespace

// 0x101f1448. The listing keeps the default handler's answer in r3 (the decompiler's text says void).
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageDestroy) {
        UI_PostMessage(window->GetParent(), kMessageClose, 0, 0);
        UIMain_DecideKbCapture();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// A button message (0x100) with one of the three button ids calls that button's callback with the argument (a NULL callback counts as
// having answered 1); a nonzero answer closes the dialog (message 4) and the answer is 0. Any other id, a zero answer, and every other
// message but 0x14 go to the default handler. When the dialog is destroyed (0x14) the key tables are put back as the dialog found them
// (parameter 4: -1 leaves them, 0 enables all, anything else disables all) and, if the dialog paused the game (parameter 5), the game and
// the timers are unpaused unless the game's word at 0x3c is set already or the lobby runs; then the parent is told to close and the
// keyboard is decided again.
int32_t DialogBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageButton) {
        int16_t button = -1;
        if (a == kFirstButton + 1) {
            button = 1;
        } else if (a < kFirstButton + 1) {
            if (a > kFirstButton - 1) button = 0;
        } else if (a < kFirstButton + 3) {
            button = 2;
        }
        if (button >= 0) {
            void* argument = window->getExtraParam(3);
            dlgcallback callback = reinterpret_cast<dlgcallback>(window->getExtraParam(button));
            const int32_t answer = callback == nullptr ? 1 : callback(argument);
            if (answer != 0) {
                UI_SendMessage(window, kMessageClose, 0, 0);
                return 0;
            }
        }
    } else if (message == kMessageDestroy) {
        if (ExtraWord(window, 4) != -1) {
            if (static_cast<uint32_t>(ExtraWord(window, 4)) == 0) UIMain_EnableAllKeyTables();
            else UIMain_DisableAllKeyTables();
        }
        if (static_cast<uint32_t>(ExtraWord(window, 5)) != 0 && gGame.mF3C == 0 && gLobby == nullptr) {
            gGame.mF3C = 1;
            gGame.Unpause();
            UI_UnPauseTimerSystem();
        }
    }
    if (message == kMessageDestroy) {
        UI_PostMessage(window->GetParent(), kMessageClose, 0, 0);
        UIMain_DecideKbCapture();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10182304: the opposite of UIMain_DisableAllKeyTables (src/ui_dialogbox.cpp), which sets the byte to 1.
void UIMain_EnableAllKeyTables() {
    EnableAllKeytables();
    gUIMainKeyTablesDisabled = 0;
}

// 0x101d8ba0: undoes UI_PauseTimerSystem. Only when the timers are paused: they are let run, and every timer in use (the list from
// gUITimerList) is restarted from the clock's tick now, so that no time passed in the pause counts (the tick is asked once per timer).
void UI_UnPauseTimerSystem() {
    if (gUITimersPaused != 0) {
        gUITimersPaused = 0;
        for (InterfaceTimer* timer = gUITimerList; timer != nullptr; timer = timer->mNext) timer->mStart = IveGotTicks();
    }
}
