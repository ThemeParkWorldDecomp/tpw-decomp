// CPausableTime (main app): Pause, UnPause, TogglePause, GetPauseFlag and ReadPausableTime
// at 0x10154268..0x1015445c (src/game/PausableTime.cpp). Inline construction recovered from
// __sinit_Clock_cpp at 0x101476dc.
#pragma once
#include "CVariableTime.h"

class CPausableTime {
public:
    CPausableTime() : mPausedAt(0), mPausedDuration(0), mPauseFlag(0) {}
    void Pause();                  // 0x10154268
    void UnPause();                // 0x101542e0
    void TogglePause();            // 0x1015436c
    int32_t GetPauseFlag();        // 0x10154420
    int32_t ReadPausableTime();    // 0x1015445c: the variable time less the time spent paused

    CVariableTime mVariableTime;   // 0x00
    int32_t mPausedAt;             // 0x20  the reading when the pause began
    int32_t mPausedDuration;       // 0x24  the time spent paused so far
    int32_t mPauseFlag;            // 0x28
};
static_assert(sizeof(CPausableTime) == 0x2c, "CPausableTime layout");
