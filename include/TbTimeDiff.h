// TbTimeDiff.h: a time span in 100 ns units (Windows FILETIME ticks), bullfrog library,
// functions at 0x100157bc..0x1001603c. The library names no file here; TbTimeDiff.cpp
// follows the class. The Mac stores the 64-bit value big-endian (high word at +0); the
// translation keeps an int64_t and the tests compare values.
//
// The operators returning a TbTimeDiff or TbTimeStamp by value take the hidden result
// buffer in r3, this in r4 and the operand in r5; Set and Get return a TbError the same
// way (this in r4, the five arguments in r5..r9).
#pragma once
#include <cstdint>

#include "TbError.h"

class TbTimeStamp;
// w217: the formatter accepts narrow and Mac UTF-16 string templates.
template<class Char> class TbDynamicStringTemplate;
template<class Char> class TbStaticStringTemplate;

class TbTimeDiff {
public:
    TbTimeDiff operator+(const TbTimeDiff& other) const;    // 0x100157bc
    TbTimeStamp operator+(const TbTimeStamp& other) const;  // 0x10015818
    TbTimeDiff operator-(const TbTimeDiff& other) const;    // 0x10015874
    TbTimeDiff& operator+=(const TbTimeDiff& other);        // 0x100158d0
    TbTimeDiff& operator-=(const TbTimeDiff& other);        // 0x1001592c
    TbTimeDiff& operator*=(int32_t factor);                 // 0x10015988
    TbTimeDiff& operator*=(float factor);                   // 0x100159e0
    TbTimeDiff& operator/=(int32_t divisor);                // 0x10015a5c
    TbTimeDiff& operator/=(float divisor);                  // 0x10015ad0
    bool operator<(const TbTimeDiff& other) const;          // 0x10015b4c
    bool operator<=(const TbTimeDiff& other) const;         // 0x10015bb4
    bool operator>(const TbTimeDiff& other) const;          // 0x10015c20
    bool operator>=(const TbTimeDiff& other) const;         // 0x10015c88
    bool operator!=(const TbTimeDiff& other) const;         // 0x10015cf4
    bool operator==(const TbTimeDiff& other) const;         // 0x10015d58
    // The span of the given days, hours, minutes, seconds and milliseconds, summed in 32
    // bits (so it wraps as the original does) before scaling to ticks.
    TbError Set(int32_t days, int32_t hours, int32_t minutes, int32_t seconds,
                int32_t milliseconds);                      // 0x10015db8
    // Whole days, then hours (0..23), minutes, seconds (0..59) and milliseconds
    // (0..999), each only if its pointer is not null; negative spans give negative parts.
    TbError Get(int32_t* days, int32_t* hours, int32_t* minutes, int32_t* seconds,
                int32_t* milliseconds) const;               // 0x10015ec4

    int64_t mTicks;   // 0x00
};

namespace NS_MacDoze { struct tagFILETIME; }
struct TbFileStamp;

// PARTIAL: bullfrog TbTimeStamp (LbMaths.cpp range), a point in time in the same units.
class TbTimeStamp {
public:
    // bullfrog 0x10026660 (172 bytes, src/TbTimeStamp_calendar.cpp): sets the stamp to the given calendar time (local time converted by
    // NS_MacDoze::SystemTimeToFileTime); the result (0, or -1 when the date does not convert) comes back in a hidden buffer and the
    // callers seen so far (CParkTime::CParkTime and Reset) ignore it. The seventh argument is passed on the stack.
    TbError SetTime(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t millisecond);

    // Unit w216 (src/TbTimeStamp_calendar.cpp).
    TbError AddMonths(int32_t months);                           // 0x100268a4
    TbError AddYears(int32_t years);                             // 0x10026a24
    TbTimeStamp& operator=(NS_MacDoze::tagFILETIME& fileTime);    // 0x1002640c
    TbTimeStamp& operator=(const TbFileStamp& stamp);             // 0x1002637c
    TbTimeStamp& operator+=(const TbTimeDiff& diff);              // 0x10026020
    TbTimeStamp& operator-=(const TbTimeDiff& diff);              // 0x100260a8
    TbTimeStamp operator-(const TbTimeDiff& diff) const;          // 0x10025f68
    bool operator<(const TbTimeStamp& other) const;              // 0x10026130
    bool operator<=(const TbTimeStamp& other) const;             // 0x10026190
    bool operator>=(const TbTimeStamp& other) const;             // 0x10026254
    bool operator!=(const TbTimeStamp& other) const;             // 0x100262b8
    bool operator==(const TbTimeStamp& other) const;             // 0x1002631c

    // Unit w123b: two more bullfrog imports (stand-ins, called through main-app glue). operator+ is bullfrog 0x10025f0c (36 bytes, glue 0x10239958,
    // import __pl__11TbTimeStampCFRC10TbTimeDiff; translated in src/TbTimeStamp_ops.cpp, unit w125): the sum of the stamp and the span, the hidden result in r3, this in r4, the span in r5.
    TbTimeStamp operator+(const TbTimeDiff& diff) const;
    // Unit w124 stand-ins, translated in src/TbTimeStamp_ops.cpp (unit w125): bullfrog 0x100261f4 (glue 0x1023b1e8): true when this stamp is later than `other` as an UNSIGNED 64-bit count (the answer is 0 or 1 in the low byte of r3);
    // bullfrog 0x10025fc4 (glue 0x10239988): the span from `other` to this stamp, the hidden result in r3, this in r4, `other` in r5.
    bool operator>(const TbTimeStamp& other) const;
    TbTimeDiff operator-(const TbTimeStamp& other) const;
    // GetTime is bullfrog 0x10026740 (296 bytes, glue 0x10239970, import GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi): the calendar fields of the stamp
    // (year, month, day, day of week, hour, minute, second, millisecond; each only if its pointer is not NULL); the hidden TbError comes back in r3,
    // this in r4, the first six outputs in r5..r10 and the last two on the stack (+0x38, +0x3c). Translated in src/TbTimeStamp_ops.cpp (unit w125):
    // a failing NS_MacDoze::FileTimeToSystemTime gives TbError -1 and writes nothing.
    TbError GetTime(int32_t* year, int32_t* month, int32_t* day, int32_t* dayOfWeek, int32_t* hour, int32_t* minute, int32_t* second,
                    int32_t* millisecond) const;

    // Unit w217 (src/TbTimeStamp_format.cpp), bullfrog LbMaths.cpp range.
    int32_t Format(char* buffer, int32_t size, const char* format) const;                  // 0x100264a8, 392 B
    int32_t Format(TbDynamicStringTemplate<char>* text, const char* format) const;         // 0x10026bc0, 52 B
    int32_t Format(TbDynamicStringTemplate<char16_t>* text, const char16_t* format) const; // 0x10026c40, 52 B
    int32_t Format(TbStaticStringTemplate<char>* text, const char* format) const;          // 0x10026cc0, 52 B
    int32_t Format(TbStaticStringTemplate<char16_t>* text, const char16_t* format) const;  // 0x10026d40, 52 B

    int64_t mTicks;   // 0x00
};
