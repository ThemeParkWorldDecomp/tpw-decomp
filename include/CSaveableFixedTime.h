// CSaveableFixedTime (main app): ReadSaveableFixedTime, Save, Load and AdjustTime at
// 0x101573f8..0x101575c8 (src/game/save/SaveLoad.cpp). Inline construction recovered from
// __sinit_Clock_cpp at 0x101476dc.
#pragma once
#include "CPausableFixedTime.h"

class CSaveableFixedTime {
public:
    CSaveableFixedTime() : mAdjustment(0) {}
    int32_t ReadSaveableFixedTime();   // 0x10157550
    int32_t Save(void* file);          // 0x101573f8
    int32_t Load(void* file);          // 0x101574ac
    void AdjustTime();                 // 0x101575c8

    CPausableFixedTime mPausableTime;   // 0x00
    int32_t mAdjustment;               // 0x18
    uint32_t mSavedTime;               // 0x1c  deliberately untouched by construction
};
static_assert(sizeof(CSaveableFixedTime) == 0x20, "CSaveableFixedTime layout");
