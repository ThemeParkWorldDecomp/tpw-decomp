// LbTime_GetTime 0x100468e4 and LbTime_GetDate 0x10046968: fill the caller's structure from
// NS_MacDoze::GetLocalTime. The SYSTEMTIME is read by its Windows offsets (eight 16-bit
// fields: year 0, month 2, day of week 4, day 6, hour 8, minute 10, second 12, ms 14).
#include <cstdint>

#include "LbTime.h"
#include "platform/macdoze.h"

void LbTime_GetTime(TbTime* time) {
    uint16_t st[8];
    NS_MacDoze::GetLocalTime(reinterpret_cast<NS_MacDoze::_SYSTEMTIME*>(st));
    time->mHour = st[4];
    time->mMinute = st[5];
    time->mSecond = st[6];
    time->mMillisecond = st[7];
}

void LbTime_GetDate(TbDate* date) {
    uint16_t st[8];
    NS_MacDoze::GetLocalTime(reinterpret_cast<NS_MacDoze::_SYSTEMTIME*>(st));
    date->mDay = st[3];
    date->mMonth = st[1];
    date->mYear = st[0];
    date->mDayOfWeek = st[2];
}
