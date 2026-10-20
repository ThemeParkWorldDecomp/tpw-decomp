// CParkTime (main app of SimTheme Park 2.04b3; the inventory files it under Peep.cpp, unreliable; unit w113): the constructor and Reset.
#include "ParkTime.h"

#include "platform/macdoze.h"

// 0x10111f14 (164 bytes). The stores come in this order: +4, +0, +0xc, +8, +0x1c, Reset, +0x10, +0x14, +0x18, then the local time
// goes into mSessionStart. SYSTEMTIME's 16-bit fields are zero-extended into SetTime's arguments (lhz); wDayOfWeek is skipped.
CParkTime::CParkTime() {
    mFunnyTimeStart[1] = 0;
    mFunnyTimeStart[0] = 0;
    mSessionStart[1] = 0;
    mSessionStart[0] = 0;
    mFunnySecsPerRealSec = 15000;
    Reset();
    mMonthAtLastUpdate = 0;
    mDayAtLastUpdate = -1;
    mF18 = 0;
    NS_MacDoze::_SYSTEMTIME now;
    NS_MacDoze::GetLocalTime(&now);
    SessionStart()->SetTime(now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
}

// 0x1011275c (76 bytes): the funny time starts at 2000-01-01 00:00:00.000.
void CParkTime::Reset() {
    FunnyTimeStart()->SetTime(2000, 1, 1, 0, 0, 0, 0);
}
