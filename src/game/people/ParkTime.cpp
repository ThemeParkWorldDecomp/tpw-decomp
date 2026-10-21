// ParkTime.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of ParkTime code, named after it.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "analyser.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "DateMessages.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "Game.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "platform/macdoze.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>

// CParkTime (main app of SimTheme Park 2.04b3; the inventory files it under Peep.cpp, unreliable; unit w113): the constructor and Reset.

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

// CParkTime::Serialise (0x10111fe0, 516 bytes): the park time to or from a save file. 0 on success, 1 on the first failure. The version must be 0 or 1,
// otherwise nothing is done and the answer is 0 (the same for save and load). The last argument is unused.

int32_t CParkTime::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    // the names are "mFunnyTimeStart", "mSessionStart", "mMonthAtLastUpdate", "mDayAtLastUpdate", "mFunnySecsPerRealSec"
    TbTimeStamp* funny = FunnyTimeStart();
    TbTimeStamp* session = SessionStart();
    MacULong* rate = reinterpret_cast<MacULong*>(&mFunnySecsPerRealSec);
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVARSTRUCT<TbTimeStamp>(file, funny, "mFunnyTimeStart", 0) != 0) return 1;
            if (SAVEVARSTRUCT<TbTimeStamp>(file, session, "mSessionStart", 0) != 0) return 1;
            if (SAVEVAR<int>(file, &mMonthAtLastUpdate, "mMonthAtLastUpdate", 0) != 0) return 1;
            if (SAVEVAR<int>(file, &mDayAtLastUpdate, "mDayAtLastUpdate", 0) != 0) return 1;
            if (SAVEVAR<MacULong>(file, rate, "mFunnySecsPerRealSec", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT<TbTimeStamp>(file, funny, "mFunnyTimeStart", 0) != 0) return 1;
            if (LOADVARSTRUCT<TbTimeStamp>(file, session, "mSessionStart", 0) != 0) return 1;
            if (LOADVAR<int>(file, &mMonthAtLastUpdate, "mMonthAtLastUpdate", 0) != 0) return 1;
            if (LOADVAR<int>(file, &mDayAtLastUpdate, "mDayAtLastUpdate", 0) != 0) return 1;
            if (LOADVAR<MacULong>(file, rate, "mFunnySecsPerRealSec", 0) != 0) return 1;
        }
    }
    return 0;
}

// CParkTime::Init (0x10112694, 160 bytes), CParkTime::Update (0x10112214, 752 bytes) and CParkTime::GetFunnyTimeSinceSessionStart (0x10112b14, 300 bytes),
// main app, unit w129. The park clock keeps the calendar date of the funny time in the global gParkDate (BSS 0x1034c388) and tells the world and the UI
// when the day, the month and the year change. Every funny time asked for here goes through CParkTime::GetFunnyTime (0x101127d0, src/ParkTime_funny.cpp).

ParkDate gParkDate;

namespace {
// The lazy message centre, as every poster in the original has it: the allocation (operator new with nothrow, which the Mac calls as operator new and
// constructs the 29 headers of only when it answered a block) is tried before EACH post, and the global takes the answer even when it is NULL.
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}  // namespace

// One GetFunnyTime fills the first seven words of gParkDate (second, minute, hour, day, month, year and day of week; the millisecond is not asked for) and
// the year then loses 1900, the month 1 and the day is taken away 1 and given 1 again (two stores, kept: the compiler of the original did not fold them).
// A day different from the one kept (signed compare) posts a CMsgEndOfDay and then gives the UI the whole nine word block by value, copied AFTER the post.
// A month different from the one kept (the month 0 to 11) posts a CMsgEndOfMonth and tells the status, finance and loans panels the month + 1 (the
// global read again for each). A second GetFunnyTime asks for the raw year alone; a year different from the one kept posts a CMsgEndOfYear. Last, the
// month, the day and the raw year are kept (the globals read again).
void CParkTime::Update() {
    GetFunnyTime(&gParkDate.mYear, &gParkDate.mMonth, &gParkDate.mDay, &gParkDate.mDayOfWeek, &gParkDate.mHour, &gParkDate.mMinute, &gParkDate.mSecond,
                 nullptr);
    gParkDate.mYear -= 1900;
    gParkDate.mDay -= 1;
    gParkDate.mDay += 1;
    gParkDate.mMonth -= 1;
    if (gParkDate.mDay != mDayAtLastUpdate) {
        CMsgEndOfDay message;
        MessageCentreForPost()->PostMessage(message);
        gUIInterface.DateChange(gParkDate);
    }
    if (gParkDate.mMonth != mMonthAtLastUpdate) {
        CMsgEndOfMonth message;
        MessageCentreForPost()->PostMessage(message);
        UI_ParkStatusPanelDateChange(gParkDate.mMonth + 1);
        UI_FinancePanelPanelDateChange(gParkDate.mMonth + 1);
        UI_LoansPanelDateChange(gParkDate.mMonth + 1);
    }
    int32_t year;
    GetFunnyTime(&year, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (year != mF18) {
        CMsgEndOfYear message;
        MessageCentreForPost()->PostMessage(message);
    }
    mMonthAtLastUpdate = gParkDate.mMonth;
    mDayAtLastUpdate = gParkDate.mDay;
    mF18 = year;
}

// The park's date messages (main app, unit w129): the three constructors 0x10151c24, 0x10151c6c and 0x10151cb4, the GetType functions 0x10112de0, 0x10112da8
// and 0x10112d70, and the destructors 0x1011261c, 0x101125a4 and 0x1011252c. They live together because the native vtable needs a key function (GetType);
// the pattern is src/ThingMessages.cpp's.

CMsgEndOfYear::~CMsgEndOfYear() {}
CMsgEndOfMonth::~CMsgEndOfMonth() {}
// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgEndOfDay::~CMsgEndOfDay() {}

// The month kept is the local calendar month now minus 1 (the funny time's month; 0 to 11 when the month is 1 to 12) and the year kept is the raw year,
// each from its own GetFunnyTime (the first asks for the month alone, the second for the year alone, straight into the member). The day kept (+0x14) is
// not touched. The time stamps answered are ignored.
void CParkTime::Init() {
    int32_t month;
    GetFunnyTime(nullptr, &month, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    mMonthAtLastUpdate = month - 1;
    GetFunnyTime(&mF18, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

// 0x1011275c (76 bytes): the funny time starts at 2000-01-01 00:00:00.000.
void CParkTime::Reset() {
    FunnyTimeStart()->SetTime(2000, 1, 1, 0, 0, 0, 0);
}

// CParkTime::GetFunnyTime, main app 0x101127d0 (280 bytes; the inventory files it under Peep.cpp, unreliable), unit w123b.

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

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

// 0x10112928 (132 bytes): turns times the funny seconds per real second as an unsigned 64-bit product, divided by 4 (signed 64-bit
// division, as the runtime's routine does), times 10,000,000 ticks.
TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    const uint64_t product = static_cast<uint64_t>(turns) * mFunnySecsPerRealSec;
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff span;
    span.mTicks = quarter * 10000000;
    return span;
}

// CParkTime::GetSeason, main app 0x101129e8 (256 bytes; the inventory files it under Peep.cpp, unreliable), unit w123c.

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

// The same sum as GetFunnyTime (this class is a CWorldSavvy in the original): rate (read after GetWorld) times the world's turn as 64 bits, divided by 4
// (signed, helper 0x10237e1c), times 10,000,000 ticks, added to the funny time start; the sum's calendar time is asked for with no outputs (GetTime's
// TbError ignored, the stamp read again afterwards); the answer is that stamp minus the stamp at offset 0 of `this` (the funny time start; the listing
// does not use the session start at +8).
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    const uint32_t turn = world.mTurn;
    const uint64_t product = static_cast<uint64_t>(mFunnySecsPerRealSec) * static_cast<uint64_t>(turn);
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff elapsed;
    elapsed.mTicks = static_cast<int64_t>(static_cast<uint64_t>(quarter) * 10000000u);
    const TbTimeStamp sum = *FunnyTimeStart() + elapsed;
    TbTimeStamp when = sum;
    when.GetTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    return when - *FunnyTimeStart();
}

// Unit v51034: CPerson::GetHeadSpriteID, SetSpotAnim, ScoreOnCloserInspection, FinalDestroy, LOADVAR<ESP_Sprites>, Pilot::~Pilot,
// CParkTime::GetFunnyTimeSinceGameTurn. The person is reached by Mac offset.

// 0x101161a8
template<> int32_t LOADVAR<ESP_Sprites>(void* file, ESP_Sprites* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x10112c80
TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    const uint32_t turns = world.mTurn - turn;
    const uint64_t product = static_cast<uint64_t>(turns) * static_cast<uint64_t>(mFunnySecsPerRealSec);
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>(static_cast<uint64_t>(quarter) * 10000000u);
    return span;
}
