// Unit w216, bullfrog LbMaths.cpp range. Calendar conversion remains a platform call.
#include "TbTimeDiff.h"
#include "LbFile.h"
#include "platform/macdoze.h"

// 0x10026660 (172 bytes). wDayOfWeek is deliberately not assigned.
TbError TbTimeStamp::SetTime(int32_t year, int32_t month, int32_t day, int32_t hour,
                            int32_t minute, int32_t second, int32_t millisecond) {
    NS_MacDoze::_SYSTEMTIME time;
    time.wYear = static_cast<uint16_t>(year);
    time.wMonth = static_cast<uint16_t>(month);
    time.wDay = static_cast<uint16_t>(day);
    time.wHour = static_cast<uint16_t>(hour);
    time.wMinute = static_cast<uint16_t>(minute);
    time.wSecond = static_cast<uint16_t>(second);
    time.wMilliseconds = static_cast<uint16_t>(millisecond);
    NS_MacDoze::tagFILETIME ft;
    if (NS_MacDoze::SystemTimeToFileTime(&time, &ft) == 0 || ft.dwHighDateTime >= 0x80000000u) return TbError(-1);
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return TbError(0);
}

// 0x100268a4 (336 bytes). The original shifts the time fields by one place.
TbError TbTimeStamp::AddMonths(int32_t months) {
    int32_t year, month, day, weekday, hour, minute, second;
    TbError result = GetTime(&year, &month, &day, &weekday, &hour, &minute, &second, nullptr);
    if (static_cast<int32_t>(result) < 0) return result;
    month = static_cast<int32_t>(static_cast<uint32_t>(month) + static_cast<uint32_t>(months));
    const int32_t quotient = month / 12;
    year = static_cast<int32_t>(static_cast<uint32_t>(year) + static_cast<uint32_t>(quotient));
    month = static_cast<int32_t>(static_cast<uint32_t>(month) - static_cast<uint32_t>(quotient) * 12u);
    return SetTime(year, month, day, weekday, hour, minute, second);
}

// 0x10026a24 (368 bytes). Multiply and add wrap in 32 bits before signed division.
TbError TbTimeStamp::AddYears(int32_t years) {
    int32_t year, month, day, weekday, hour, minute, second;
    TbError result = GetTime(&year, &month, &day, &weekday, &hour, &minute, &second, nullptr);
    if (static_cast<int32_t>(result) < 0) return result;
    month = static_cast<int32_t>(static_cast<uint32_t>(month) + static_cast<uint32_t>(years) * 12u);
    const int32_t quotient = month / 12;
    year = static_cast<int32_t>(static_cast<uint32_t>(year) + static_cast<uint32_t>(quotient));
    month = static_cast<int32_t>(static_cast<uint32_t>(month) - static_cast<uint32_t>(quotient) * 12u);
    return SetTime(year, month, day, weekday, hour, minute, second);
}

// 0x1002640c (88 bytes).
TbTimeStamp& TbTimeStamp::operator=(NS_MacDoze::tagFILETIME& ft) {
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return *this;
}

// 0x1002637c (88 bytes).
TbTimeStamp& TbTimeStamp::operator=(const TbFileStamp& stamp) {
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(stamp.mHigh) << 32) | stamp.mLow);
    return *this;
}

// 0x10026020 (80 bytes). Only the sign of the wrapped result controls the restore.
TbTimeStamp& TbTimeStamp::operator+=(const TbTimeDiff& diff) {
    const uint64_t value = static_cast<uint64_t>(mTicks) + static_cast<uint64_t>(diff.mTicks);
    if (value < 0x8000000000000000ull) mTicks = static_cast<int64_t>(value);
    return *this;
}

// 0x100260a8 (80 bytes).
TbTimeStamp& TbTimeStamp::operator-=(const TbTimeDiff& diff) {
    const uint64_t value = static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(diff.mTicks);
    if (value < 0x8000000000000000ull) mTicks = static_cast<int64_t>(value);
    return *this;
}

// 0x10025f68 (36 bytes). This overload has no sign check.
TbTimeStamp TbTimeStamp::operator-(const TbTimeDiff& diff) const {
    return TbTimeStamp{static_cast<int64_t>(static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(diff.mTicks))};
}

// 0x10026130 (40 bytes).
bool TbTimeStamp::operator<(const TbTimeStamp& other) const {
    return static_cast<uint64_t>(mTicks) < static_cast<uint64_t>(other.mTicks);
}
// 0x10026190 (44 bytes).
bool TbTimeStamp::operator<=(const TbTimeStamp& other) const {
    return static_cast<uint64_t>(mTicks) <= static_cast<uint64_t>(other.mTicks);
}
// 0x10026254 (44 bytes).
bool TbTimeStamp::operator>=(const TbTimeStamp& other) const {
    return static_cast<uint64_t>(mTicks) >= static_cast<uint64_t>(other.mTicks);
}
// 0x100262b8 (44 bytes).
bool TbTimeStamp::operator!=(const TbTimeStamp& other) const { return mTicks != other.mTicks; }
// 0x1002631c (40 bytes).
bool TbTimeStamp::operator==(const TbTimeStamp& other) const { return mTicks == other.mTicks; }
