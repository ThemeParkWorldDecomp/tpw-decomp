// CParkTime::Init (0x10112694, 160 bytes), CParkTime::Update (0x10112214, 752 bytes) and CParkTime::GetFunnyTimeSinceSessionStart (0x10112b14, 300 bytes),
// main app, unit w129. The park clock keeps the calendar date of the funny time in the global gParkDate (BSS 0x1034c388) and tells the world and the UI
// when the day, the month and the year change. Every funny time asked for here goes through CParkTime::GetFunnyTime (0x101127d0, src/ParkTime_funny.cpp).
#include <new>

#include "CMessageCentre.h"
#include "DateMessages.h"
#include "ParkTime.h"
#include "UIInterface.h"
#include "WorldSavvy.h"
#include "ui_main.h"

ParkDate gParkDate;

namespace {
// The lazy message centre, as every poster in the original has it: the allocation (operator new with nothrow, which the Mac calls as operator new and
// constructs the 29 headers of only when it answered a block) is tried before EACH post, and the global takes the answer even when it is NULL.
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}  // namespace

// The month kept is the local calendar month now minus 1 (the funny time's month; 0 to 11 when the month is 1 to 12) and the year kept is the raw year,
// each from its own GetFunnyTime (the first asks for the month alone, the second for the year alone, straight into the member). The day kept (+0x14) is
// not touched. The time stamps answered are ignored.
void CParkTime::Init() {
    int32_t month;
    GetFunnyTime(nullptr, &month, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    mMonthAtLastUpdate = month - 1;
    GetFunnyTime(&mF18, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

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
