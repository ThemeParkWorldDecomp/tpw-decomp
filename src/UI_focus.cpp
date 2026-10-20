// UI_Base.cpp's window activation and held mouse buttons (0x101ca5e0,
// 0x101ca658, 0x101ca944). Names of the shared globals are inferred.
#include "UI_Base.h"

InterfaceWindow* gUIRootWindow;   // 0x10395c24, filled by UI_InitSystem
int32_t gUIActive = 1;           // 0x102af924, original initialised data
int16_t gUIButtonFlags;          // 0x10395bbe

void UI_Activate() {
    if (gUIRootWindow != nullptr && gUIActive == 0) {
        activateUI(gUIRootWindow, 1);
        gUIActive = 1;
    }
}

void UI_DeActivate() {
    if (gUIRootWindow != nullptr && gUIActive != 0) {
        activateUI(gUIRootWindow, 0);
        gUIActive = 0;
    }
    // Clear held mouse buttons even when there is no root window or it was inactive.
    gUIButtonFlags = 0;
}

int16_t UI_GetButtonFlags() { return gUIButtonFlags; }
