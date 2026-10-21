// CPausableFixedTime (main app): Pause, UnPause, TogglePause and ReadPausableFixedTime at
// 0x1015400c..0x101541d0 (src/game/PausableTime.cpp). It has no GetPauseFlag. Inline construction
// recovered from __sinit_Clock_cpp at 0x101476dc.
#pragma once
#include "CFixedTime.h"

class CPausableFixedTime {
public:
    CPausableFixedTime() : mPausedAt(0), mPausedDuration(0), mPauseFlag(0) {}
    void Pause();                      // 0x1015400c
    void UnPause();                    // 0x10154088
    void TogglePause();                // 0x10154118
    int32_t ReadPausableFixedTime();   // 0x101541d0: the fixed time less the time spent paused

    CFixedTime mFixedTime;      // 0x00
    int32_t mPausedAt;         // 0x0c  the reading when the pause began
    int32_t mPausedDuration;   // 0x10  the time spent paused so far
    int32_t mPauseFlag;        // 0x14
};
static_assert(sizeof(CPausableFixedTime) == 0x18, "CPausableFixedTime layout");
