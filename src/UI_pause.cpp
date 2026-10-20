// UIInterface::Pause (main app, SimTheme Park 2.04b3, ui_ErrorDisplay.cpp's range; unit w81). Its mirror, UnPause, is in src/UI_pump.cpp.
#include "UIInterface.h"

#include "ui_main.h"

// 0x101ef8a0 (36 bytes): PauseUI(); `this` is not used.
void UIInterface::Pause() { PauseUI(); }
