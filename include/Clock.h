// Clock.cpp: main app CClock methods at 0x101472f4..0x10147754.
// The two time objects are represented by composition while their owners are partial.
#pragma once
#include <cstddef>
#include "CSaveableTime.h"
#include "CSaveableFixedTime.h"

class CClock {
public:
    CClock() = default;   // inline construction in __sinit_Clock_cpp, 0x101476dc
    int32_t Read();
    int32_t ReadFixed();
    void Pause();
    void UnPause();
    void TogglePause();
    int32_t GetPauseFlag();
    int32_t Save(void* file);
    int32_t Load(void* file);
    void PreSave();
    void PostLoad();

    CSaveableTime mTime;             // 0x00
    CSaveableFixedTime mFixedTime;    // 0x44
};
static_assert(offsetof(CClock, mFixedTime) == 0x44, "CClock fixed time offset");
static_assert(sizeof(CClock) == 0x64, "CClock layout");

extern CClock gClock;   // main app BSS 0x1037f048, original name not recovered
