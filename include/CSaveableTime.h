// CSaveableTime (main app): ReadSaveableTime, Save, Load and AdjustTime at
// 0x1015763c..0x101577fc (src/GameClock.cpp). Inline construction recovered from
// __sinit_Clock_cpp at 0x101476dc.
#pragma once
#include "CFixableTime.h"
#include "CPausableTime.h"

class CSaveableTime : public CFixableTime {
public:
    CSaveableTime() : mFixedFlag(0), mFixedAt(0), mFixedAdjustment(0), mAdjustment(0) {}
    int32_t ReadSaveableTime();   // 0x1015778c
    int32_t Save(void* file);     // 0x1015763c: mSavedTime as four bytes; 1 when all four went
    int32_t Load(void* file);     // 0x101576ec
    void AdjustTime();            // 0x101577fc: makes the clock read mSavedTime now

    CPausableTime mPausableTime;   // 0x00
    int32_t mFixedFlag;            // 0x2c  CFixableTime state: 1 while the clock is held in fixed steps
    int32_t mFixedAt;              // 0x30  the reading held while fixed
    uint32_t mFixedStep;           // 0x34  milliseconds a frame (deliberately untouched by construction)
    int32_t mFixedAdjustment;      // 0x38  carries the reading across a fixed spell
    int32_t mAdjustment;           // 0x3c  moves the whole clock to a loaded time
    uint32_t mSavedTime;           // 0x40  the reading Save writes (deliberately untouched by construction)
};
static_assert(sizeof(CSaveableTime) == 0x44, "CSaveableTime layout");
