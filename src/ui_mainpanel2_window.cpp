// ui_mainpanel2.cpp (main app 0x101a8504, 0x101a86d0, 0x101a87c4; unit w88): the pause window. (HidePauseWindow and gPauseWindow are in
// ui_mainpanel2_pause.cpp.)
#include "Game.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

// 0x101a86d0 (76 bytes): makes the pause window (Dialog with the pause script and message function, modal) when there is none. Dialog may
// answer NULL, and then there is still none.
void ShowPauseWindow() {
    if (gPauseWindow == nullptr) gPauseWindow = Dialog(gPauseDialogDescription, PauseWindowMsg, 1);
}

// 0x101a87c4 (128 bytes): makes the window when there is none, else closes it: message 4, the pointer cleared, the keyboard capture decided.
void TogglePauseWindow() {
    if (gPauseWindow == nullptr) {
        gPauseWindow = Dialog(gPauseDialogDescription, PauseWindowMsg, 1);
    } else {
        UI_SendMessage(gPauseWindow, 4, 0, 0);
        gPauseWindow = nullptr;
        UIMain_DecideKbCapture();
    }
}

// 0x101a8504 (400 bytes). Message 0x14: the window is forgotten (no other message clears it). Message 0x15: the window's rectangle, with
// the animation's offsets taken off, gives the pivot: the halves of the RIGHT and BOTTOM edges (the two shorts at +8 and +10 of the
// rectangle, shifted right arithmetically), and the animation starts with 0x23. Message 0x1000b (a key): when the system key table is off
// (its enabled word is 0), the table is searched for the key (the low 16 bits of `a`) with 1 as the modifiers when bits 16..23 of `b` are 0,
// else 0; a non-zero answer (as a short) toggles the game's pause (false: not all sounds). Every message ends in the default dialog
// callback, whose answer is the answer (the Mac's decompile says void; r3 is left as the callback answered it).
int32_t PauseWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) {
        gPauseWindow = nullptr;
    } else if (message == 0x15) {
        CoolRect rect;
        window->GetPosition(&rect, 1);
        window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
        window->startAnim(gPauseAnim, 0x23);
    } else if (message == 0x1000b) {
        if (gKeyTable->mEnabled == 0) {
            const int16_t found =
                static_cast<int16_t>(gKeyTable->Search(static_cast<int16_t>(a), ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0 ? 1 : 0));
            if (found != 0) gGame.TogglePause(false);
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}
