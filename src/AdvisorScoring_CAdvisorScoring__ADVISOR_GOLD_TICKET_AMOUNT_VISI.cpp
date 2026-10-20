// CAdvisorScoring scorers (main app, unit v5106): the gold ticket amount scorers, the gold ticket explanation and the tooltip tutorial.
// Mac offsets of the balance values read: see SlotOf (Mac offset of a scoring-object slot -> mBalance.mValues index).
#include <cstdint>

#include "AdvisorScoring.h"
#include "History.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "World.h"
#include "analyser.h"

namespace {

constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }

}  // namespace

// 0x1001b604 (128 bytes): the day number (the time since the session start over 864000000000 ticks, the low word of the 64-bit quotient)
// strictly between the values at 0x5e8 and 0x5ec gives the value at 0x5f0.
int32_t CAdvisorScoring::_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t day = static_cast<int32_t>(since.mTicks / 864000000000LL);
    if (day > mBalance.mValues[SlotOf(0x5e8)].mInteger && day < mBalance.mValues[SlotOf(0x5ec)].mInteger)
        return mBalance.mValues[SlotOf(0x5f0)].mInteger;
    return 0;
}

// 0x1001b6d0 (92 bytes): the value at 0x5f4 while the analyser's history at +0x216d8 holds exactly one value.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_EXPLANATION() {
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() == 1) return mBalance.mValues[SlotOf(0x5f4)].mInteger;
    return 0;
}

// 0x1001b8ec (136 bytes): park open and the percentage of golden ticket objects researched in (0x5fc, 0x604] gives 0x600.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x5fc)].mInteger && percent <= mBalance.mValues[SlotOf(0x604)].mInteger)
        return mBalance.mValues[SlotOf(0x600)].mInteger;
    return 0;
}

// 0x1001b9d0 (136 bytes): (0x604, 0x60c] gives 0x608.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x604)].mInteger && percent <= mBalance.mValues[SlotOf(0x60c)].mInteger)
        return mBalance.mValues[SlotOf(0x608)].mInteger;
    return 0;
}

// 0x1001bab4 (124 bytes): above 0x60c gives 0x610.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x60c)].mInteger) return mBalance.mValues[SlotOf(0x610)].mInteger;
    return 0;
}

namespace {

// The shared body of the three VISITORS scorers (identical code apart from the two slots): the visitors in the park as a percentage of
// the main balance value at 0x754 (zero: 0), clamped to 100 (the original asks the analyser a second time when it does not exceed 100).
int32_t VisitorsAmount(const CAdvisorScoring* self, int32_t thresholdSlot, int32_t amountSlot) {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t* total = &gMainBalance.mValues[(0x754 - 8) / 4].mInteger;
    if (*total == 0) return 0;
    int32_t percent;
    if (gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() * 100 / *total > 100) {
        percent = 100;
    } else {
        percent = gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() * 100 / *total;
    }
    if (percent > self->mBalance.mValues[thresholdSlot].mInteger) return self->mBalance.mValues[amountSlot].mInteger;
    return 0;
}

}  // namespace

// 0x1001bb8c (228 bytes): thresholds 0x614 / amount 0x618.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a() { return VisitorsAmount(this, SlotOf(0x614), SlotOf(0x618)); }

// 0x1001bcc0 (228 bytes): 0x61c / 0x620.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b() { return VisitorsAmount(this, SlotOf(0x61c), SlotOf(0x620)); }

// 0x1001bdf4 (228 bytes): 0x624 / 0x628.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c() { return VisitorsAmount(this, SlotOf(0x624), SlotOf(0x628)); }
