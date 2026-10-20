// ui_mainpanel2.cpp (main app, SimTheme Park 2.04b3): the pause window's global and HidePauseWindow. The window itself is made and handled
// in ui_mainpanel2_window.cpp (unit w88: ShowPauseWindow, TogglePauseWindow, PauseWindowMsg).
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"

// 0x103949cc (the original name is unknown): the pause window, set by ShowPauseWindow and TogglePauseWindow; NULL when none is up.
InterfaceWindow* gPauseWindow;

// 0x101a8744 (88 bytes). Sends the pause window message 4 (its close) when there is one, forgets it (the pointer is cleared even
// when there was none) and has the keyboard capture decided again. UnPauseUI calls it. The Mac clears the global through the
// address it loaded before the send, which is the same word.
void HidePauseWindow() {
    if (gPauseWindow != nullptr) UI_SendMessage(gPauseWindow, 4, 0, 0);
    gPauseWindow = nullptr;
    UIMain_DecideKbCapture();
}
