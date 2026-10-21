// TbTimeStamp.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbTimeStamp code, named after it.

#include "LbFile.h"
#include "platform/macdoze.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include <cstring>

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

TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    return TbTimeStamp{static_cast<int64_t>(static_cast<uint64_t>(mTicks) + static_cast<uint64_t>(diff.mTicks))};
}

// Unit w216, bullfrog LbMaths.cpp range. Calendar conversion remains a platform call.

// 0x10025f68 (36 bytes). This overload has no sign check.
TbTimeStamp TbTimeStamp::operator-(const TbTimeDiff& diff) const {
    return TbTimeStamp{static_cast<int64_t>(static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(diff.mTicks))};
}

TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    return TbTimeDiff{static_cast<int64_t>(static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(other.mTicks))};
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

// 0x1002637c (88 bytes).
TbTimeStamp& TbTimeStamp::operator=(const TbFileStamp& stamp) {
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(stamp.mHigh) << 32) | stamp.mLow);
    return *this;
}

// 0x1002640c (88 bytes).
TbTimeStamp& TbTimeStamp::operator=(NS_MacDoze::tagFILETIME& ft) {
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return *this;
}

// Bullfrog shared, LbMaths.cpp range, unit w217: TbTimeStamp::Format 0x100264a8
// (392 B), 0x10026bc0/0x10026c40/0x10026cc0/0x10026d40 (52 B each);
// TemplatedDynamicFormat char 0x10026f7c (364 B), UTF-16 0x10026dc0 (320 B);
// TemplatedStaticFormat char 0x10028da8 (416 B), UTF-16 0x10028fc0 (360 B);
// ProcessFormatTag char 0x10027f88 (3,536 B), UTF-16 0x10027164 (3,540 B).

namespace {
bool IsLeadUnit(char c) { return (c & 0x80) != 0; }
bool IsLeadUnit(char16_t) { return false; }
}

template<class Char> void ProcessFormatTag(Char** out, int32_t* room, const Char** format,
                                         NS_MacDoze::_SYSTEMTIME time) {
    const auto write = [&](Char c) { *(*out)++ = c; };
    const auto pair = [&](uint32_t value, Char* digits) {
        digits[0] = static_cast<Char>(value / 10 + '0');
        digits[1] = static_cast<Char>(value % 10 + '0');
    };
    const auto single = [&](Char* digits) {
        Char* p = digits[0] == '0' ? digits + 1 : digits;
        if (*room > 1 && digits[0] != '0') {
            write(*p++);
            --*room;
        }
        write(*p);
        --*room;
    };
    // The failing room test decrements it too, even when n has reached zero.
    const auto copyDigits = [&](const Char* p, int32_t n) {
        while ((*room)-- != 0 && n-- != 0) write(*p++);
    };
    const auto copyName = [&](const char* p) {
        int32_t n = static_cast<int32_t>(std::strlen(p));
        while ((*room)-- != 0 && n-- != 0) write(static_cast<Char>(static_cast<signed char>(*p++)));
    };
    switch (**format) {
    case '%':
        ++*format;
        write('%');
        --*room;
        break;
    case 'd': {
        Char digits[2];
        pair(time.wDay, digits);
        Char* p = digits;
        if (*room > 1) {
            if ((*format)[1] == 'd') {
                write(*p++);
                --*room;
            } else {
                if (*p != '0') {
                    write(*p);
                    --*room;
                }
                ++p;
            }
        }
        write(*p);
        --*room;
        ++*format;
        if (**format == 'd') ++*format;
        break;
    }
    case 'w':
        if ((*format)[1] == 'w' && (*format)[2] == 'w') {
            NS_MacDoze::_SYSTEMTIME copy = time;
            char name[30];
            const bool full = (*format)[3] == 'w';
            NS_MacDoze::GetDateFormatA(0x800, 0, &copy, full ? "dddd" : "ddd", name, 30);
            *format += full ? 4 : 3;
            copyName(name);
        } else {
            write(static_cast<Char>(time.wDayOfWeek + '0'));
            --*room;
            ++*format;
        }
        break;
    case 'm': {
        Char digits[2];
        pair(time.wMonth, digits);
        if ((*format)[1] == 'm' && (*format)[2] == 'm') {
            NS_MacDoze::_SYSTEMTIME copy = time;
            copy.wYear = 1980;
            char name[30];
            const bool full = (*format)[3] == 'm';
            NS_MacDoze::GetDateFormatA(0x800, 0, &copy, full ? "MMMM" : "MMM", name, 30);
            *format += full ? 4 : 3;
            copyName(name);
        } else if ((*format)[1] == 'm') {
            Char* p = digits;
            if (*room > 1) {
                write(*p++);
                --*room;
            }
            write(*p);
            --*room;
            *format += 2;
        } else {
            single(digits);
            ++*format;
        }
        break;
    }
    case 'y': {
        const uint32_t year = time.wYear;
        Char digits[5] = {static_cast<Char>(year / 10000 + '0'),
                          static_cast<Char>(year / 1000 % 10 + '0'),
                          static_cast<Char>(year / 100 % 10 + '0'),
                          static_cast<Char>(year / 10 % 10 + '0'),
                          static_cast<Char>(year % 10 + '0')};
        if ((*format)[1] != 'y') break;
        if ((*format)[2] == 'y' && (*format)[3] == 'y') {
            const bool skip = digits[0] == '0';
            copyDigits(digits + (skip ? 1 : 0), skip ? 4 : 5);
            *format += 4;
        } else {
            Char* p = digits + 3;
            if (*room > 1) {
                write(*p++);
                --*room;
            }
            write(*p);
            --*room;
            *format += 2;
        }
        break;
    }
    case 'h': {
        const bool twice = (*format)[1] == 'h';
        const bool twelve = (*format)[twice ? 2 : 1] == 'a';
        uint32_t hour = time.wHour;
        if (twelve) {
            if (hour > 12) hour -= 12;
            if (hour == 0) hour = 12;
        }
        Char digits[2];
        pair(hour, digits);
        *format += (twice ? 2 : 1) + (twelve ? 1 : 0);
        if (twice) {
            Char* p = digits;
            if (*room > 1) {
                write(*p++);
                --*room;
            }
            write(*p);
            --*room;
        } else {
            single(digits);
        }
        break;
    }
    case 'n':
    case 's': {
        const Char tag = **format;
        Char digits[2];
        pair(tag == 'n' ? time.wMinute : time.wSecond, digits);
        if ((*format)[1] == tag) {
            Char* p = digits;
            if (*room > 1) write(*p++);
            write(*p);
            // The original does not account for either character in room.
            *format += 2;
        } else {
            single(digits);
            ++*format;
        }
        break;
    }
    case 'l': {
        const uint32_t ms = time.wMilliseconds;
        Char digits[3] = {static_cast<Char>(ms / 100 % 10 + '0'),
                          static_cast<Char>(ms / 10 % 10 + '0'),
                          static_cast<Char>(ms % 10 + '0')};
        Char* p = digits;
        int32_t n = 3;
        if ((*format)[1] == 'l' && (*format)[2] == 'l') {
            *format += 3;
        } else {
            if (digits[0] == '0') {
                n = 2;
                ++p;
                if (digits[1] == '0') {
                    n = 1;
                    ++p;
                }
            }
            ++*format;
        }
        copyDigits(p, n);
        break;
    }
    case 'a': {
        NS_MacDoze::_SYSTEMTIME copy = time;
        copy.wYear = 1980;
        char name[10];
        const bool full = (*format)[1] == 'm';
        NS_MacDoze::GetTimeFormatA(0x800, 0, &copy, full ? "tt" : "t", name, 10);
        *format += full ? 2 : 1;
        copyName(name);
        ++*format;
        break;
    }
    default:
        break;
    }
}

int32_t TbTimeStamp::Format(char* buffer, int32_t size, const char* format) const {
    int32_t room = size - 1;
    const uint64_t ticks = static_cast<uint64_t>(mTicks);
    NS_MacDoze::tagFILETIME fileTime{static_cast<uint32_t>(ticks), static_cast<uint32_t>(ticks >> 32)};
    NS_MacDoze::_SYSTEMTIME time;
    NS_MacDoze::FileTimeToSystemTime(&fileTime, &time);
    while (room > 0) {
        const signed char c = static_cast<signed char>(*format);
        if (c == '%') {
            ++format;
            ProcessFormatTag(&buffer, &room, &format, time);
        } else if (c == 0) {
            break;
        } else {
            if ((c & 0x80) != 0 && room > 1) {
                *buffer++ = *format++;
                --room;
            }
            *buffer++ = *format++;
            --room;
        }
    }
    *buffer = 0;
    return room > 0 ? size - room - 1 : 0;
}

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

template<class Str, class Char> int32_t TemplatedStaticFormat(Str* text, const Char* format,
                                                           const TbTimeStamp* stamp) {
    const uint64_t ticks = static_cast<uint64_t>(stamp->mTicks);
    NS_MacDoze::tagFILETIME fileTime{static_cast<uint32_t>(ticks), static_cast<uint32_t>(ticks >> 32)};
    NS_MacDoze::_SYSTEMTIME time;
    NS_MacDoze::FileTimeToSystemTime(&fileTime, &time);
    int32_t count = 0;
    Char buffer[30];
    while (*format != 0) {
        if (*format == '%') {
            int32_t room = 30;
            ++format;
            Char* cursor = buffer;
            ProcessFormatTag(&cursor, &room, &format, time);
            *cursor = 0;
            *text += static_cast<const Char*>(buffer);
            if (text->mTruncated != 0) break;
            count += 30 - room;
        } else {
            if (IsLeadUnit(*format)) {
                // Original DBCS bug: moves the string object, rather than appending.
                text += static_cast<signed char>(*format++);
                if (text->mTruncated != 0) break;
                ++count;
            }
            *text += *format++;
            if (text->mTruncated != 0) break;
            ++count;
        }
    }
    return text->mTruncated != 0 ? 0 : count;
}

template<class Str, class Char> int32_t TemplatedDynamicFormat(Str* text, const Char* format,
                                                            const TbTimeStamp* stamp) {
    const uint64_t ticks = static_cast<uint64_t>(stamp->mTicks);
    NS_MacDoze::tagFILETIME fileTime{static_cast<uint32_t>(ticks), static_cast<uint32_t>(ticks >> 32)};
    NS_MacDoze::_SYSTEMTIME time;
    NS_MacDoze::FileTimeToSystemTime(&fileTime, &time);
    int32_t count = 0;
    Char buffer[30];
    while (*format != 0) {
        if (*format == '%') {
            int32_t room = 30;
            ++format;
            Char* cursor = buffer;
            ProcessFormatTag(&cursor, &room, &format, time);
            *cursor = 0;
            *text += static_cast<const Char*>(buffer);
            count += 30 - room;
        } else {
            if (IsLeadUnit(*format)) {
                ++count;
                // Original DBCS bug, as in the static formatter.
                text += static_cast<signed char>(*format++);
            }
            *text += *format++;
            ++count;
        }
    }
    return count;
}

int32_t TbTimeStamp::Format(TbDynamicStringTemplate<char>* text, const char* format) const {
    return TemplatedDynamicFormat(text, format, this);
}
int32_t TbTimeStamp::Format(TbDynamicStringTemplate<char16_t>* text, const char16_t* format) const {
    return TemplatedDynamicFormat(text, format, this);
}
int32_t TbTimeStamp::Format(TbStaticStringTemplate<char>* text, const char* format) const {
    return TemplatedStaticFormat(text, format, this);
}
int32_t TbTimeStamp::Format(TbStaticStringTemplate<char16_t>* text, const char16_t* format) const {
    return TemplatedStaticFormat(text, format, this);
}

template void ProcessFormatTag(char**, int32_t*, const char**, NS_MacDoze::_SYSTEMTIME);
template void ProcessFormatTag(char16_t**, int32_t*, const char16_t**, NS_MacDoze::_SYSTEMTIME);
template int32_t TemplatedStaticFormat(TbStaticStringTemplate<char>*, const char*, const TbTimeStamp*);
template int32_t TemplatedStaticFormat(TbStaticStringTemplate<char16_t>*, const char16_t*, const TbTimeStamp*);
template int32_t TemplatedDynamicFormat(TbDynamicStringTemplate<char>*, const char*, const TbTimeStamp*);
template int32_t TemplatedDynamicFormat(TbDynamicStringTemplate<char16_t>*, const char16_t*, const TbTimeStamp*);
