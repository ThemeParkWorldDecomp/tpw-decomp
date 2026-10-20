// CParkTime::GetSeason, main app 0x101129e8 (256 bytes; the inventory files it under Peep.cpp, unreliable), unit w123c.
#include "ParkTime.h"
#include "WorldSavvy.h"

#include <cstring>

// The season of the park's funny time now. This is the first half of CParkTime::GetFunnyTime (0x101127d0, unit w123b): the funny time is the
// stored start plus (rate * the world's turn / 4) * 10,000,000 ticks, where the clock reaches the world through CWorldSavvy::GetWorld called
// with the clock itself as `this` (GetWorld ignores `this` and answers gWorld). The unsigned rate (the word at +0x1c, 15000 after construction)
// and turn (world +0x1da70c) are multiplied to 64 bits; the quotient by 4 is the compiler's SIGNED 64-bit divide (helper 0x10237e1c), so a
// product with bit 63 set truncates toward zero from below; the quotient is scaled after the division and wraps in 64 bits. The sum
// TbTimeStamp::operator+ makes (start + span) is copied to a second local, which TbTimeStamp::GetTime reads for the month only (all seven other
// outputs are NULL). GetTime's error is ignored: when it fails the month is not written and stays whatever the stack held (undefined in the
// original, and here). The answer is month / 3 (rounding toward zero), with no offset and no clamp: January and February are 0, March 1,
// December 4.
int32_t CParkTime::GetSeason() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    const uint32_t turn = world.mTurn;
    const uint64_t product = static_cast<uint64_t>(mFunnySecsPerRealSec) * static_cast<uint64_t>(turn);
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff elapsed;
    elapsed.mTicks = static_cast<int64_t>(static_cast<uint64_t>(quarter) * 10000000u);
    TbTimeStamp start;
    std::memcpy(&start, mFunnyTimeStart, sizeof start);
    const TbTimeStamp sum = start + elapsed;
    TbTimeStamp when = sum;
    int32_t month;
    when.GetTime(nullptr, &month, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    return month / 3;
}
