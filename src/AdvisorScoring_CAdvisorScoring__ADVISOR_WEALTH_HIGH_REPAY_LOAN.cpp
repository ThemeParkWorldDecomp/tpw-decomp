// Unit v5896: twelve more advisor scorers (pricing, wealth, wages, staff counts, queues, toilets, features).
// Mac offsets of the balance values read are in the calls of Bal (a Mac offset in the scoring object).
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"

#include "History.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

uint16_t gAdvisorNoRideQueue;   // 0x1027df04

namespace {

// The value slot at a Mac offset of the scoring object (the balance table starts at 0x0c, its values at 0x14, slot 0 reserved).
inline int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }

inline CThing::ThingModel Model(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// 100 ns ticks in a day and in a month (the original divides with the 64-bit runtime routine at 0x10237e1c and keeps the low word).
constexpr int64_t kTicksPerMonth = 0x1792F8648000LL;
constexpr int64_t kTicksPerDay = 0xC92A69C000LL;

// CHistory<int,144>::GetAverageOfLast (0x1001c064) leaves the count it averaged in r3, and the callers test it; the translation of that
// callee answers nothing, so the count is taken from GetSumOfLast and the mean made as the callee makes it.
inline int32_t AverageOfLast(CHistory<int32_t, 144>& history, int32_t& out, int32_t count) {
    double sum;
    const int32_t n = history.GetSumOfLast(sum, count);
    if (n > 0) out = static_cast<int32_t>(ppc_fctiwz(sum / static_cast<double>(n)));
    else out = 0;
    return n;
}

}  // namespace

// 0x10016218: balance minus the loans outstanding, above the multiplier (0x338) times what the cheapest loan still owes: 0x33c.
int32_t CAdvisorScoring::_ADVISOR_WEALTH_HIGH_REPAY_LOAN() {
    const int32_t balance = gGameWorldRef->GetBankAccount()->GetBalance();
    const uint32_t total = gGameWorldRef->GetBankAccount()->TotalOutstandingOnLoans();
    const int32_t spare = static_cast<int32_t>(static_cast<uint32_t>(balance) - total);
    const int32_t cheapest = static_cast<int32_t>(gGameWorldRef->GetBankAccount()->GetCheapestLoanTaken());
    if (cheapest < 0) return 0;
    const uint32_t owed = gGameWorldRef->GetBankAccount()->OutstandingOnLoan(static_cast<uint32_t>(cheapest));
    const float limit = mBalance.mValues[SlotOf(0x338)].mFloat * static_cast<float>(owed);
    if (static_cast<float>(spare) > limit) return mBalance.mValues[SlotOf(0x33c)].mInteger;
    return 0;
}

// 0x10015cec: a complete set of opinions of the tickets and an average above 0x2f8: 0x2fc.
int32_t CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_CHEAP() {
    if (gGameWorld->GetParkAnalyser()->HaveCompleteSetOfOpinions(0)) {
        const int32_t average = gGameWorld->GetParkAnalyser()->GetAverageOpinionOn(0);
        if (average > mBalance.mValues[SlotOf(0x2f8)].mInteger) return mBalance.mValues[SlotOf(0x2fc)].mInteger;
    }
    return 0;
}

// 0x10015c14: a complete set of opinions of the tickets and an average below 0x2f0: 0x2f4.
int32_t CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_EXPENSIVE() {
    if (gGameWorld->GetParkAnalyser()->HaveCompleteSetOfOpinions(0)) {
        const int32_t average = gGameWorld->GetParkAnalyser()->GetAverageOpinionOn(0);
        if (average < mBalance.mValues[SlotOf(0x2f0)].mInteger) return mBalance.mValues[SlotOf(0x2f4)].mInteger;
    }
    return 0;
}

// 0x10016778: at least 3 months into the session, the mean of the wages history (0x1f7f0) over the last 0x358 values above the mean of the
// other history (0x1fc90) times 0x354: 0x35c.
int32_t CAdvisorScoring::_ADVISOR_WAGES_HIGH() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t months = static_cast<int32_t>(since.mTicks / kTicksPerMonth);
    if (months < 3) return 0;
    int32_t wages;
    int32_t other;
    if (AverageOfLast(gGameWorld->GetParkAnalyser()->mHistory1F100[3], wages, mBalance.mValues[SlotOf(0x358)].mInteger) == 0) return 0;
    if (AverageOfLast(gGameWorld->GetParkAnalyser()->mHistory1F100[5], other, mBalance.mValues[SlotOf(0x358)].mInteger) == 0) return 0;
    const float limit = static_cast<float>(other) * mBalance.mValues[SlotOf(0x354)].mFloat;
    if (static_cast<float>(wages) > limit) return mBalance.mValues[SlotOf(0x35c)].mInteger;
    return 0;
}

// 0x10016a84: more mechanics than 0x370 plus the number of rides: 0x374.
int32_t CAdvisorScoring::_ADVISOR_LOTS_MECHANICS() {
    const int32_t limit = mBalance.mValues[SlotOf(0x370)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model(4)) > limit) return mBalance.mValues[SlotOf(0x374)].mInteger;
    return 0;
}

// 0x10016b50: more guards than 0x378 plus the number of rides: 0x37c.
int32_t CAdvisorScoring::_ADVISOR_LOTS_SECURITY() {
    const int32_t limit = mBalance.mValues[SlotOf(0x378)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model(7)) > limit) return mBalance.mValues[SlotOf(0x37c)].mInteger;
    return 0;
}

// 0x10016c1c: more researchers than 0x380 plus the number of rides: 0x384.
int32_t CAdvisorScoring::_ADVISOR_LOTS_RESEARCHERS() {
    const int32_t limit = mBalance.mValues[SlotOf(0x380)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model(8)) > limit) return mBalance.mValues[SlotOf(0x384)].mInteger;
    return 0;
}

// 0x10016cec: a ride with an unconnected queue (kept in mWord4): 0x388.
int32_t CAdvisorScoring::_ADVISOR_QUEUE_UNCONNECTED() {
    mWord4 = gGameWorld->GetParkAnalyser()->FindRideWithUnconnectedQueue(false).mIndex;
    if (mWord4 != gAdvisorNoRideQueue) return mBalance.mValues[SlotOf(0x388)].mInteger;
    return 0;
}

// 0x10016db0: when gMainBalance's word at 0x2a8 equals 0x38c and as many handymen as that: 0x390.
int32_t CAdvisorScoring::_ADVISOR_MAX_HANDYMEN() {
    const int32_t limit = mBalance.mValues[SlotOf(0x38c)].mInteger;
    if (gMainBalance.mValues[(0x2a8 - 0x8) / 4].mInteger != limit) return 0;
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model(5)) >= mBalance.mValues[SlotOf(0x38c)].mInteger)
        return mBalance.mValues[SlotOf(0x390)].mInteger;
    return 0;
}

// 0x10015564: the average toilet queue is read and compared with 0x2c8, but both ways of the branch answer 0 (the original's bug).
int32_t CAdvisorScoring::_ADVISOR_FEATURES_NOT_ENOUGH_TOILETS() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    (void)gGameWorldRef->GetParkAnalyser()->GetAverageToiletQueueLenght();
    return 0;
}

// 0x100153e0: gates open, no toilets, and a share of visitors (percent) needing one above 0x2bc: 0x2c0.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_NO_TOILETS() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfToiletsInPark() != 0) return 0;
    const int32_t needing = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsNeedingToilet(
        static_cast<uint8_t>(mBalance.mValues[SlotOf(0x2b8)].mInteger));
    const int32_t inPark = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark();
    if (inPark == 0) return 0;
    const int32_t percent = static_cast<int32_t>(static_cast<uint32_t>(needing) * 100u) / inPark;
    if (percent > mBalance.mValues[SlotOf(0x2bc)].mInteger) return mBalance.mValues[SlotOf(0x2c0)].mInteger;
    return 0;
}

// 0x10015298: sideshows built, enough days since the last build, no features built: 0x2b0 plus 0x2b4 per day since the session started.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_BUILD_FEATURES() {
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kTicksPerDay);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x2b0)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x2b4)].mInteger) * static_cast<uint32_t>(days));
}
