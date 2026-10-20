// LbTime: PARTIAL bullfrog library clock (Draw.cpp range: GetTime 0x100468e4, GetDate
// 0x10046968, GetClock 0x100469ec). GetTime and GetDate fill the structure from
// NS_MacDoze::GetLocalTime (a Windows SYSTEMTIME); r3 still holds the argument on
// return, which no caller seen so far uses.
#pragma once
#include <cstdint>

struct TbTime {
    int32_t mHour;          // 0x00
    int32_t mMinute;        // 0x04
    int32_t mSecond;        // 0x08
    int32_t mMillisecond;   // 0x0c
};

struct TbDate {
    int32_t mDay;           // 0x00
    int32_t mMonth;         // 0x04
    int32_t mYear;          // 0x08
    int32_t mDayOfWeek;     // 0x0c
};

void LbTime_GetTime(TbTime* time);
void LbTime_GetDate(TbDate* date);
// The game clock in its own units (CVanillaTime saves and compares it).
uint32_t LbTime_GetClock();   // 0x100469ec
