// CFixableTime, the game clock's fixed-step mode (main app, Game.cpp range): UseFixedTime,
// CancelFixedTime, NextFrame and ReadFixableTime at 0x101479c0..0x10147b58 (src/game/FixableTime.cpp).
// Its state is CSaveableTime's mFixedFlag..mFixedAdjustment (0x2c..0x38), so it is an empty
// base of CSaveableTime here and its members reach the state through that class; `this` is
// the CSaveableTime, as in the original (GameSys.cpp calls it on gClock.mTime).
#pragma once
#include <cstdint>

class CFixableTime {
public:
    void UseFixedTime(uint32_t step);   // 0x101479c0: freezes the clock, `step` frames a second
    void CancelFixedTime();             // 0x10147a58
    void NextFrame();                   // 0x10147ae0: one frame step on, unless paused
    int32_t ReadFixableTime();          // 0x10147b58
};
