// CVariableTime (main app): the constructor at 0x1016886c (src/game/VariableTime.cpp, the
// inventory's FS_MasterSave.cpp range) and the rate members and ReadVariableTime at
// 0x101688d4..0x10168a3c (src/game/VariableTime.cpp). Packed to 4 bytes like CFixedTime.
//
// The constructor builds a CFixedTime at +0 (a call of CFixedTime::CFixedTime on the
// object) and sets the rate to 1.0; it leaves the reading and the milliseconds at +0x0c and
// +0x10 alone, and ReadVariableTime never touches the CFixedTime: it keeps its own pair
// there (gClock is zero-filled static storage). The CFixedTime is storage here, not a
// member: a member would make every constructor of this class (the test libraries' fakes
// among them) build one more object.
#pragma once
#include <cstddef>
#include <cstdint>

#include "CFixedTime.h"

#pragma pack(push, 4)
class CVariableTime {
public:
    CVariableTime();
    void SetTimeRate(double rate);   // 0x101688d4
    void IncreaseTimeRate();         // 0x10168910: times 1.25, kept within 0.25 .. 2.0
    void DecreaseTimeRate();         // 0x10168988: divided by 1.25, kept within 0.25 .. 2.0
    double GetTimeRate();            // 0x10168a00
    uint32_t ReadVariableTime();     // 0x10168a3c

    uint8_t mFixedTimeStorage[sizeof(CFixedTime)];   // 0x00  a CFixedTime the constructor builds, never read here
    uint32_t mLastClock;        // 0x0c  LbTime_GetClock at the last reading
    double mMilliseconds;       // 0x10  the milliseconds so far, scaled by the rate
    double mRate;               // 0x18  1.0 at construction
};
#pragma pack(pop)
static_assert(offsetof(CVariableTime, mLastClock) == 0x0c, "CVariableTime reading offset");
static_assert(offsetof(CVariableTime, mMilliseconds) == 0x10, "CVariableTime milliseconds offset");
static_assert(offsetof(CVariableTime, mRate) == 0x18, "CVariableTime rate offset");
static_assert(sizeof(CVariableTime) == 0x20, "CVariableTime layout");
