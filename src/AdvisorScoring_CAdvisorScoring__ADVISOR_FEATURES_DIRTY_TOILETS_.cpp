// Unit v5081: five advisor scorers about toilets, the choice of features and the sideshows (CAdvisorScoring, main app).
// The balance values are read by their Mac offset in the scoring object; the host's slots are 8 bytes, so the slot is (offset - 0x14) / 4.
#include <cstdint>

#include "AdvisorScoring.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

// The four toilet ids the scorers compare the filthiest toilet with (never written in the original: zero).
uint16_t gAdvisorNoToilet1 = 0;   // 0x1027defc
uint16_t gAdvisorNoToilet2 = 0;   // 0x1027defe
uint16_t gAdvisorNoToilet3 = 0;   // 0x1027df00
uint16_t gAdvisorNoToilet4 = 0;   // 0x1027df02

namespace {

constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }

}  // namespace

// 0x10015628: no score with the park closed or a staff type 5 present, or the filthiest toilet being the "none" id, or the mean filth
// outside [0x2d0, 0x2e0]; else the mean times the value at 0x2d4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_1() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet1) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2d0)].mInteger) return 0;
    if (average > mBalance.mValues[SlotOf(0x2e0)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2d4)].mInteger;
}

// 0x10015798: the same, but with staff type 5 absent giving 0; limits 0x2d8 and 0x2e8, factor 0x2d4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_2() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) == 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet2) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2d8)].mInteger) return 0;
    if (average > mBalance.mValues[SlotOf(0x2e8)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2d4)].mInteger;
}

// 0x10015908: staff type 5 present gives 0; lower limit 0x2e0 only, factor 0x2e4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_3() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet3) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2e0)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2e4)].mInteger;
}

// 0x10015a64: staff type 5 absent gives 0; lower limit 0x2e8 only, factor 0x2ec.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_4() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) == 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet4) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2e8)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2ec)].mInteger;
}

// 0x10015018: with features available, enough days since the last build (0x634), enough feature objects (0x2a4) and a share of feature
// types below 0x2a0 percent (unsigned): the value at 0x2a8.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesAvailable(false));
    if (available == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjFeature, 0) < mBalance.mValues[SlotOf(0x2a4)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x2a0)].mInteger))
        return mBalance.mValues[SlotOf(0x2a8)].mInteger;
    return 0;
}

// 0x10014ea0: sideshows exist, enough days since the last build (0x634), sideshow types fewer than the ride types times the float at
// 0x294, and a balance above 0x298: the value at 0x29c.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS() {
    const int32_t sideshows = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false);
    if (sideshows == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    const int32_t rides = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    const float rideCount = static_cast<float>(rides);
    const float sideshowCount = static_cast<float>(sideshows);
    const float limit = rideCount * mBalance.mValues[SlotOf(0x294)].mFloat;
    if (!(sideshowCount < limit)) return 0;
    if (gGameWorldRef->GetBankAccount()->GetBalance() > mBalance.mValues[SlotOf(0x298)].mInteger)
        return mBalance.mValues[SlotOf(0x29c)].mInteger;
    return 0;
}
