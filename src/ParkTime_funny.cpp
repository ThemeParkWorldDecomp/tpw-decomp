// CParkTime::GetFunnyTime, main app 0x101127d0 (280 bytes; the inventory files it under Peep.cpp, unreliable), unit w123b.
#include "ParkTime.h"
#include "WorldSavvy.h"

// The park's accelerated clock. The funny time is the stored start plus (rate * the world's turn / 4) * 10,000,000 ticks (100 ns each): the rate is
// the clock's word at +0x1c (15000 after construction), the turn the unsigned word at world +0x1da70c, and the clock reaches the world through
// CWorldSavvy::GetWorld called with the clock itself as `this` (the clock is a CWorldSavvy in the original; GetWorld ignores `this` and answers gWorld).
//
// The two unsigned words are multiplied to 64 bits (mullw / mulhwu), then divided by 4 with the compiler's signed 64-bit divide (helper 0x10237e1c):
// a product with bit 63 set (rate * turn >= 2^63) is negative there and the quotient truncates toward zero. The quotient times 10,000,000 wraps in
// 64 bits. TbTimeStamp::operator+ adds it to the start in place (the receiver is the clock itself) and the sum is copied into a second local, which
// TbTimeStamp::GetTime reads into the eight optional outputs (a NULL output is passed on, not tested here). GetTime's TbError is ignored, whatever it
// says. The answer is the second local read AFTER GetTime, so a GetTime that changed its stamp shows. Nothing in the clock or the world is written.
TbTimeStamp CParkTime::GetFunnyTime(int32_t* year, int32_t* month, int32_t* day, int32_t* dayOfWeek, int32_t* hour, int32_t* minute, int32_t* second,
                                    int32_t* millisecond) const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    const uint32_t turn = world.mTurn;
    const uint64_t product = static_cast<uint64_t>(mFunnySecsPerRealSec) * static_cast<uint64_t>(turn);
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff elapsed;
    elapsed.mTicks = static_cast<int64_t>(static_cast<uint64_t>(quarter) * 10000000u);
    const TbTimeStamp sum = *FunnyTimeStart() + elapsed;
    TbTimeStamp when = sum;
    when.GetTime(year, month, day, dayOfWeek, hour, minute, second, millisecond);
    return when;
}
