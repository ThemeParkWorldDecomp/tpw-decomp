// Unit v5973: six advisor scorers (0x10013dd8, 0x10014c00, 0x10014d54, 0x10015168, 0x10017528, 0x10017b2c).
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
constexpr int64_t kTicksPerDay = 864000000000LL;      // 0xc92a69c000
constexpr int64_t kTicksPerMonth = 25920000000000LL;  // 0x17 92f8648000
}  // namespace

// 0x10013dd8: rides available, enough days since the last build (0x634), enough rides (0x224) and a share of ride types below 0x220 percent
// (unsigned): the value at 0x228.
int32_t CAdvisorScoring::_ADVISOR_RIDES_POOR_SELECTION() {
    CParkAnalyser* a = gGameWorldRef->GetParkAnalyser();
    const uint32_t available = static_cast<uint32_t>(a->TypesOfRidesAvailable(false));
    if (available == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) < mBalance.mValues[SlotOf(0x224)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x220)].mInteger))
        return mBalance.mValues[SlotOf(0x228)].mInteger;
    return 0;
}

// 0x10014c00: a researcher (staff 8), sideshow types built, some left to research, research speed 0 and every available type built: 0x288.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_RESEARCH_MORE() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(8)) == 0) return 0;
    const int32_t built = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false);
    if (built == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfSideshowsLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(2)) != 0) return 0;
    if (built != gGameWorldRef->GetParkAnalyser()->TypesOfSideshowsAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x288)].mInteger;
}

// 0x10014d54: shops built, enough days since the last build (0x634), no sideshows: 0x28c + 0x290 * (whole days since the session start, low word).
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_SIDESHOWS() {
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kTicksPerDay);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x28c)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x290)].mInteger) * static_cast<uint32_t>(days));
}

// 0x10015168: a researcher, feature types built, some left to research and every available type built: 0x2ac.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_RESEARCH_MORE() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(8)) == 0) return 0;
    const int32_t built = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false);
    if (built == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesLeftToResearch() == 0) return 0;
    if (built != gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x2ac)].mInteger;
}

// 0x10017528: young park (months below 0x3c8, the low word of the quotient, signed) and the history (analyser + 0x216d8) fell by more than 0x3cc percent.
int32_t CAdvisorScoring::_ADVISOR_HYPE_NEWPARK() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t months = static_cast<int32_t>(since.mTicks / kTicksPerMonth);
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() <= 1) return 0;
    if (months >= mBalance.mValues[SlotOf(0x3c8)].mInteger) return 0;
    int32_t latest;
    int32_t previous;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(latest, 0)) return 0;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(previous, 1)) return 0;
    if (previous == 0) return 0;
    if (latest >= previous) return 0;
    const int32_t n = static_cast<int32_t>(static_cast<uint32_t>(latest) * 100u);
    const int32_t q = previous == -1 ? static_cast<int32_t>(0u - static_cast<uint32_t>(n)) : n / previous;
    if (100 - q > mBalance.mValues[SlotOf(0x3cc)].mInteger) return mBalance.mValues[SlotOf(0x3d0)].mInteger;
    return 0;
}

// 0x10017b2c: the latest history value above 0x410: remembers the bus stop B cell in mWord8 and answers 0x414.
int32_t CAdvisorScoring::_ADVISOR_BIG_BUSLOAD_ARRIVED() {
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() <= 0) return 0;
    int32_t latest;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(latest, 0)) return 0;
    if (latest <= mBalance.mValues[SlotOf(0x410)].mInteger) return 0;
    const auto stop = gGameWorldRef->GetMap()->GetBusStopPos(1);
    mWord8 = stop.mIndex;
    return mBalance.mValues[SlotOf(0x414)].mInteger;
}
