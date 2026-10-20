// bullfrog (Program: bullfrog), unit w125: TbTimeStamp::operator+ 0x10025f0c (36 bytes), operator- 0x10025fc4 (36 bytes), operator> 0x100261f4
// (40 bytes) and GetTime 0x10026740 (296 bytes). A stamp is a signed 64-bit count of 100 ns ticks (the Mac stores it big-endian, the high word at +0).
//
// operator+ adds and operator- subtracts the other operand's 64 bits (addc/adde, subfc/subfe): both wrap, neither tests for overflow.
// operator> is an UNSIGNED 64-bit compare (the listing's subfc/subfe/subfe/neg: carry out of `other - this`), not a signed one: a stamp with the
// top bit set is later than every positive one. The answer is 0 or 1 in the low byte of r3 (clrlwi 24).
//
// GetTime hands the stamp to NS_MacDoze::FileTimeToSystemTime as a FILETIME (the low word first, then the high word: the original gets the high
// word through the bullfrog helper 0x1006aeb4, a logical 64-bit shift right by 32). When that answers 0 the result is TbError -1 and NO output is
// written; otherwise every non-NULL output gets its 16-bit field zero-extended to 32 bits, in the order year, month, day, day of week, hour,
// minute, second, millisecond (which is the order of the stores, not of the SYSTEMTIME fields), and the result is TbError 0.
#include "TbTimeDiff.h"
#include "platform/macdoze.h"

TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    return TbTimeStamp{static_cast<int64_t>(static_cast<uint64_t>(mTicks) + static_cast<uint64_t>(diff.mTicks))};
}

TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    return TbTimeDiff{static_cast<int64_t>(static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(other.mTicks))};
}

bool TbTimeStamp::operator>(const TbTimeStamp& other) const {
    return static_cast<uint64_t>(mTicks) > static_cast<uint64_t>(other.mTicks);
}

TbError TbTimeStamp::GetTime(int32_t* year, int32_t* month, int32_t* day, int32_t* dayOfWeek, int32_t* hour, int32_t* minute,
                             int32_t* second, int32_t* millisecond) const {
    const uint64_t ticks = static_cast<uint64_t>(mTicks);
    NS_MacDoze::tagFILETIME fileTime;
    fileTime.dwLowDateTime = static_cast<uint32_t>(ticks);
    fileTime.dwHighDateTime = static_cast<uint32_t>(ticks >> 32);
    NS_MacDoze::_SYSTEMTIME time;
    if (NS_MacDoze::FileTimeToSystemTime(&fileTime, &time) == 0) return TbError(-1);
    if (year != nullptr) *year = time.wYear;
    if (month != nullptr) *month = time.wMonth;
    if (day != nullptr) *day = time.wDay;
    if (dayOfWeek != nullptr) *dayOfWeek = time.wDayOfWeek;
    if (hour != nullptr) *hour = time.wHour;
    if (minute != nullptr) *minute = time.wMinute;
    if (second != nullptr) *second = time.wSecond;
    if (millisecond != nullptr) *millisecond = time.wMilliseconds;
    return TbError(0);
}
