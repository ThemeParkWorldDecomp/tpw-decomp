// ui_main.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: UIMain_TickRidePreview (0x10181570, 52 bytes) and
// UIMain_NotEnoughDough (0x10182160, 64 bytes).
#include "MainWindowCallback.h"
#include "TPSound.h"
#include "ui_main.h"

// Ticks the ride preview the panel shows, if any.
void UIMain_TickRidePreview() {
    if (gRidePreview != nullptr) TickRidePreview(gRidePreview);
}

// The "not enough money" sound: event 0x1d of the ui category.
void UIMain_NotEnoughDough() { SOUND_SendEvent(0, gSoundBanks[3], 0x1d, 0, 0, 0); }
