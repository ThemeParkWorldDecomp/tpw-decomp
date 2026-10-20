// ui_main.cpp, main app 0x1018679c and 0x10186854 (unit w88): the pause end of the interface (UnPauseUI is in ui_main_peek.cpp).
#include "ui_main.h"
#include "ui_ErrorDisplay.h"
#include "UI_Base.h"

// 0x1018679c (60 bytes): with a root window, the timers stop and the pause window is shown.
void PauseUI() {
    if (gUIRootWindow != nullptr) {
        UI_PauseTimerSystem();
        ShowPauseWindow();
    }
}

// 0x10186854 (72 bytes): the pause window is made or closed, then the timers follow whether one is up now (no root window test).
void TogglePauseUI() {
    TogglePauseWindow();
    if (gPauseWindow == nullptr) {
        UI_UnPauseTimerSystem();
    } else {
        UI_PauseTimerSystem();
    }
}
