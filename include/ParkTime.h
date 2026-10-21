// CParkTime (Peep.cpp in the inventory, unreliable; main app): the park's clock. PARTIAL: the constructor and Reset (unit w113).
// The two time stamps are held as words and viewed through accessors, because the Mac's stamps sit at 4-byte alignment and the
// host's TbTimeStamp (an int64) would pad the class to 0x28 bytes and push the world's cell array to 0x2dc. CParkTime lies at 0x2a0
// in the world, so the stamps are 8-aligned in practice.
#pragma once
#include <cstdint>

#include "TbTimeDiff.h"

class CGoldenTicketControl;

class CParkTime {
public:
    CParkTime();     // 0x10111f14 (164 bytes, src/game/people/ParkTime.cpp)
    // 0x1011275c (76 bytes): the funny time starts at 2000-01-01 00:00:00.000; nothing else is touched.
    void Reset();
    // 0x101127d0 (280 bytes, src/game/people/ParkTime.cpp, unit w123b): the funny time now: the start plus (rate * the world's turn / 4) * 10,000,000 ticks, with
    // eight optional calendar outputs (year, month, day, day of week, hour, minute, second, millisecond; passed on to TbTimeStamp::GetTime).
    TbTimeStamp GetFunnyTime(int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*) const;

    // Unit w123c (src/game/people/ParkTime.cpp). 0x101129e8 (256 bytes): the season of the funny time now: the calendar month (1 to 12) of the start plus
    // (rate * the world's turn / 4) * 10,000,000 ticks, divided by 3 with no offset (January and February are 0, December 4).
    int32_t GetSeason() const;
    TbTimeStamp* FunnyTimeStart() { return reinterpret_cast<TbTimeStamp*>(mFunnyTimeStart); }
    const TbTimeStamp* FunnyTimeStart() const { return reinterpret_cast<const TbTimeStamp*>(mFunnyTimeStart); }   // unit w123b
    TbTimeStamp* SessionStart() { return reinterpret_cast<TbTimeStamp*>(mSessionStart); }

    // Unit w129 (src/game/people/ParkTime.cpp). 0x10112694 (160 bytes): the month at the last update is the local calendar month now minus 1 (the funny
    // time's), the year at the last update is its raw year (not minus 1900).
    void Init();
    // 0x10112214 (752 bytes): the funny time's calendar date into the global gParkDate (the year minus 1900, the month minus 1); a changed day posts a
    // CMsgEndOfDay and tells the UI (DateChange, with the whole date block), a changed month posts a CMsgEndOfMonth and tells three panels, a changed year
    // (asked of a second GetFunnyTime) posts a CMsgEndOfYear; then the month, day and raw year are kept.
    void Update();
    // 0x10112b14 (300 bytes): ((rate * the world's turn) / 4, signed) * 10,000,000 ticks added to the funny time start, the sum's calendar time asked for
    // and ignored (GetTime with no outputs), then the sum taken away from the funny time start again (TbTimeStamp::operator-, whose operand the listing
    // takes from `this`, offset 0, not the session start at 8).
    TbTimeDiff GetFunnyTimeSinceSessionStart() const;
    TbTimeDiff GetFunnyTimeForGameTurns(uint32_t turns) const;   // 0x10112928 (132 bytes, untranslated): the funny time span of that many game turns
    TbTimeDiff GetFunnyTimeSinceGameTurn(uint32_t turn) const;   // 0x10112c80 (180 bytes, untranslated): the funny time since that game turn
    // The golden ticket control the world keeps at CWorld + 0x2c0, which is this object's word at 0x20 (the world's layout does not name it).
    CGoldenTicketControl* GoldenTickets() { return reinterpret_cast<CGoldenTicketControl*>(&mF20); }

    uint32_t mFunnyTimeStart[2];    // 0x00  TbTimeStamp "mFunnyTimeStart" (cleared, then Reset sets it)
    uint32_t mSessionStart[2];      // 0x08  TbTimeStamp "mSessionStart" (cleared, then the local time)
    int32_t mMonthAtLastUpdate;     // 0x10  (cleared); month - 1 at the last Update
    int32_t mDayAtLastUpdate;       // 0x14  (-1)
    int32_t mF18;                   // 0x18  not serialised (cleared); the raw year at the last Update (unit w129)
    uint32_t mFunnySecsPerRealSec;  // 0x1c  (15000)
    uint32_t mF20;                  // 0x20  never written (assumed: CArrival starts at 0x2c4)

    // Unit w146 (untranslated): 0x10111fe0, the park time; 0 on success, 1 on failure.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
};
static_assert(sizeof(CParkTime) == 0x24, "CParkTime is 0x24 bytes");

// The calendar date CParkTime::Update fills (BSS 0x1034c388, original name not recovered): nine words laid out as a C struct tm (second, minute, hour,
// day, month, year, day of week, day of year, daylight saving flag). Update writes the first seven; UIInterface::DateChange takes the whole block by value.
struct ParkDate {
    int32_t mSecond;      // 0x00
    int32_t mMinute;      // 0x04
    int32_t mHour;        // 0x08
    int32_t mDay;         // 0x0c
    int32_t mMonth;       // 0x10  0 to 11 after Update
    int32_t mYear;        // 0x14  minus 1900 after Update
    int32_t mDayOfWeek;   // 0x18
    int32_t mDayOfYear;   // 0x1c  never written by Update
    int32_t mIsDst;       // 0x20  never written by Update
};
static_assert(sizeof(ParkDate) == 0x24, "ParkDate is nine words");
extern ParkDate gParkDate;   // 0x1034c388
