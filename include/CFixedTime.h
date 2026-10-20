// CFixedTime (main app, the inventory's Game.cpp range): the constructor at 0x10147bdc
// (src/CFixedTime.cpp) and ReadFixedTime at 0x10147c18 (src/GameClock.cpp). The Mac packs
// the class to 4 bytes, so the double sits at +4 and is not 8-aligned; the host packs the
// same way and keeps every offset.
#pragma once
#include <cstddef>
#include <cstdint>

#pragma pack(push, 4)
class CFixedTime {
public:
    CFixedTime();
    uint32_t ReadFixedTime();   // 0x10147c18

    uint32_t mLastClock;        // 0x00  LbTime_GetClock at the last reading
    double mMilliseconds;       // 0x04  the milliseconds so far, unaligned
};
#pragma pack(pop)
static_assert(offsetof(CFixedTime, mMilliseconds) == 0x04, "CFixedTime double offset");
static_assert(sizeof(CFixedTime) == 0x0c, "CFixedTime layout");
