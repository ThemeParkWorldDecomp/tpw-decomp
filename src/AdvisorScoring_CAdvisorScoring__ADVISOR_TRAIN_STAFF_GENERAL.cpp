// Eight advisor scorers (unit v5282): the staff and costume scorers 0x100188f4..0x10018f4c and the three poor value pricing scorers
// 0x100192a4..0x100194b4. Mac offsets of the balance values read are in the calls of Bal (a Mac offset in the scoring object).
#include "AdvisorScoring.h"
#include "Debug.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

uint16_t gAdvisorNoneFoodPoorValue = 0;         // 0x1027df06
uint16_t gAdvisorNoneShopsPoorValue = 0;        // 0x1027df08
uint16_t gAdvisorNoneRestaurantPoorValue = 0;   // 0x1027df0a

namespace {
inline int32_t Bal(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}
}  // namespace

int32_t CAdvisorScoring::_ADVISOR_TRAIN_STAFF_GENERAL() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() > Bal(this, 0x4d0)) {
        if (gGameWorldRef->GetStaffHQ()->mTraining.mF14 == 0) return Bal(this, 0x4d4);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_SET_PATROLS_GENERAL() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() > Bal(this, 0x4c8)) {
        if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(static_cast<CThing::ThingModel>(0)) == 0)
            return Bal(this, 0x4cc);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_HIRE_STAFF_GENERAL() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning() < Bal(this, 0x4c4)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() == 0) return Bal(this, 0x4c0);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_NEED_STAFF_ROOM() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    if (static_cast<int32_t>(since.mTicks / 0x1792F8648000LL) < 3) return 0;
    const int32_t tired = gGameWorldRef->GetParkAnalyser()->NumberOfStaffTooTiredToWork();
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfStaff();
    if (total == 0) return 0;
    if (tired * 100 / total > Bal(this, 0x4b8)) return Bal(this, 0x4bc);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_FOOD_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(static_cast<CObjectShopType>(2), id);
    if (id.mIndex == gAdvisorNoneFoodPoorValue) return 0;
    if (figure >= Bal(this, 0x4d8)) return 0;
    CTRACE(0x26, "Food poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4dc) * (figure - Bal(this, 0x4d8));
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_SHOPS_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(id);
    if (id.mIndex == gAdvisorNoneShopsPoorValue) return 0;
    if (figure >= Bal(this, 0x4e0)) return 0;
    CTRACE(0x26, "shops poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4e4) * (figure - Bal(this, 0x4e0));
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_RESTAURANT_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(static_cast<CObjectShopType>(3), id);
    if (id.mIndex == gAdvisorNoneRestaurantPoorValue) return 0;
    if (figure >= Bal(this, 0x4e8)) return 0;
    CTRACE(0x26, "restaurants poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4ec) * (figure - Bal(this, 0x4e8));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_COSTUMES() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsWithCostumes() > Bal(this, 0x4a8)) return Bal(this, 0x4ac);
    return 0;
}
