// Game.cpp's pause/resume routines (main app 0x10149904, 0x101499d0).
#include "Game.h"

#include "Advisor.h"
#include "Clock.h"
#include "TPSound.h"
#include "UIInterface.h"

void CGame::Pause(ePauseFlag flag, bool allSounds) {
    if (mF3C != 1) return;
    mF1C = 1;
    if (static_cast<int32_t>(flag) == 1) gUIInterface.Pause();
    gClock.Pause();
    mF20 = static_cast<uint8_t>(allSounds);
    if (allSounds) {
        SOUND_Pause();
    } else {
        ADV_Pause();
        SOUND_SetPauseOn3DSoundsOnly(1);
    }
}

void CGame::Unpause() {
    if (mF3C != 1) return;
    mF1C = 0;
    gUIInterface.UnPause();
    gClock.UnPause();
    if (mF20 != 0) {
        SOUND_UnPause();
    } else {
        ADV_UnPause();
        SOUND_SetPauseOn3DSoundsOnly(0);
    }
}
