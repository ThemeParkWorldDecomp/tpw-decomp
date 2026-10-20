// Unit v5939: nine more scorers of the advisor (rides, shops, sideshows). The Mac offsets of the balance values they read are in the comments;
// the balance table's slot s lies at Mac offset 0x14 + 4 * s of the scoring object.
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

namespace {

constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }

constexpr int64_t kDayTicks = 864000000000LL;           // 0xc9 2a69c000: the 100 ns ticks of a day
constexpr int64_t kThirtyDayTicks = 25920000000000LL;   // 0x1792 f8648000

constexpr CThing::ThingModel kResearcher = static_cast<CThing::ThingModel>(8);

inline CParkAnalyser* Analyser() { return gGameWorldRef->GetParkAnalyser(); }

}  // namespace

// 0x10013f28: with a researcher, rides in the park, rides left to research, no research speed on category 0 and every available ride type
// built: the value at 0x22c.
int32_t CAdvisorScoring::_ADVISOR_RIDES_RESEARCH_MORE() {
    if (Analyser()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t built = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    if (built == 0) return 0;
    if (Analyser()->TypesOfRidesLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(0)) != 0) return 0;
    if (built != Analyser()->TypesOfRidesAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x22c)].mInteger;
}

// 0x10014078: no ride built (golden tickets counted): 0x230 + 0x234 * the whole days since the session start (low 32 bits of the quotient).
int32_t CAdvisorScoring::_ADVISOR_RIDES_BUILD_RIDES() {
    if (Analyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, true) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kDayTicks);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x230)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x234)].mInteger) * static_cast<uint32_t>(days));
}

// 0x10014160: at least 0x238 periods of 30 days since the session start, a researcher, no upgrade available and no research speed on category 4: 0x23c.
int32_t CAdvisorScoring::_ADVISOR_RIDES_NO_UPGRADES() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t periods = static_cast<int32_t>(since.mTicks / kThirtyDayTicks);
    if (periods < mBalance.mValues[SlotOf(0x238)].mInteger) return 0;
    if (Analyser()->NumberOfStaffType(kResearcher) == 0) return 0;
    if (Analyser()->TypesOfUpgradesAvailable(false) > 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(4)) != 0) return 0;
    return mBalance.mValues[SlotOf(0x23c)].mInteger;
}

// 0x100142a4: a researcher, upgrades available but at most 0x240 of them, no research speed on category 4: 0x244.
int32_t CAdvisorScoring::_ADVISOR_RIDES_MORE_UPGRADES() {
    if (Analyser()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t upgrades = Analyser()->TypesOfUpgradesAvailable(false);
    if (upgrades == 0 || mBalance.mValues[SlotOf(0x240)].mInteger < upgrades) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(4)) != 0) return 0;
    return mBalance.mValues[SlotOf(0x244)].mInteger;
}

// 0x10014558: shops available, enough days since the last build (0x634), enough shops (0x25c) and a share of shop types below 0x258 percent
// (unsigned): the value at 0x260.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(Analyser()->TypesOfShopsAvailable(false));
    if (available == 0) return 0;
    if (Analyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser()->NumberOfObjectTypeInPark(kUIObjShop, 0) < mBalance.mValues[SlotOf(0x25c)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(Analyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x258)].mInteger))
        return mBalance.mValues[SlotOf(0x260)].mInteger;
    return 0;
}

// 0x100146a8: like RIDES_RESEARCH_MORE for shops (category 1, value at 0x264).
int32_t CAdvisorScoring::_ADVISOR_SHOPS_RESEARCH_MORE() {
    if (Analyser()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t built = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    if (built == 0) return 0;
    if (Analyser()->TypesOfShopsLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(1)) != 0) return 0;
    if (built != Analyser()->TypesOfShopsAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x264)].mInteger;
}

// 0x100147f8: rides built, enough days since the last build, no shop built: 0x268 + 0x26c * the whole days since the session start.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_BUILD_SHOPS() {
    if (Analyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false) == 0) return 0;
    if (Analyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kDayTicks);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x268)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x26c)].mInteger) * static_cast<uint32_t>(days));
}

// 0x1001493c: shops built, enough days since the last build, the shop count (as float) below the second count times the float at 0x270 (a NaN
// answers 0), and a balance above 0x274: the value at 0x278.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_BUILD_MORE_SHOPS() {
    const int32_t first = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    if (first == 0) return 0;
    if (Analyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    const int32_t second = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    const float a = static_cast<float>(first);
    const float limit = static_cast<float>(second) * mBalance.mValues[SlotOf(0x270)].mFloat;
    if (!(a < limit)) return 0;   // bge tests the LT bit, which a NaN leaves clear
    if (gGameWorldRef->GetBankAccount()->GetBalance() <= mBalance.mValues[SlotOf(0x274)].mInteger) return 0;
    return mBalance.mValues[SlotOf(0x278)].mInteger;
}

// 0x10014aac: like SHOPS_POOR_SELECTION for sideshows (percent 0x27c, count 0x280, value 0x284).
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(Analyser()->TypesOfSideshowsAvailable(false));
    if (available == 0) return 0;
    if (Analyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) < mBalance.mValues[SlotOf(0x280)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(Analyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x27c)].mInteger))
        return mBalance.mValues[SlotOf(0x284)].mInteger;
    return 0;
}
