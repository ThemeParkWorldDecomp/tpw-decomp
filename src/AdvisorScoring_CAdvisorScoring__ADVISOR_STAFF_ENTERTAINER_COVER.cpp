// Unit v5118: the staff scorers of the advisor (CAdvisorScoring::_ADVISOR_STAFF_*), 0x10012b0c .. 0x10013340.
// Staff types: 4 mechanic, 5 handyman, 6 entertainer, 7 guard. The balance values are read by their Mac offset in the scoring object
// (the balance table's value slot 0 is at 0x14, four bytes a slot on the Mac).
#include <cstdint>

#include "AdvisorScoring.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "ThingParts.h"
#include "World.h"
#include "analyser.h"

namespace {

int32_t StaffSlot(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}

CThing::ThingModel Model(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// The time since the session start in units of 0x1792f8648000 ticks, the low word (the runtime's 64-bit divide 0x10237e1c).
int32_t SessionUnits() {
    TbTimeDiff t = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    return static_cast<int32_t>(t.mTicks / 0x1792F8648000LL);
}

// The scorer common to the three SET_*_AREA functions.
int32_t SetArea(CAdvisorScoring* self, int32_t type, int32_t timeOff, int32_t minOff, int32_t maxOff, int32_t mulOff) {
    if (SessionUnits() < StaffSlot(self, timeOff)) return 0;
    int32_t count = gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(type));
    if (count < StaffSlot(self, minOff)) return 0;
    int32_t withArea = gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(Model(type));
    int32_t percent = withArea * 100 / count;
    if (percent > StaffSlot(self, maxOff)) return 0;
    return (100 - percent) * StaffSlot(self, mulOff);
}

// The scorer common to the four TRAIN_* functions: hqOff is the training word of the headquarters (0xc + 4 * index).
int32_t Train(CAdvisorScoring* self, int32_t type, int32_t hqIndex, int32_t limitOff, int32_t scoreOff) {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(type)) == 0) return 0;
    if (static_cast<int32_t>(gGameWorldRef->GetStaffHQ()->mTraining.mF00[hqIndex]) > 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model(type)) < StaffSlot(self, limitOff))
        return StaffSlot(self, scoreOff);
    return 0;
}

}  // namespace

// 0x10012b0c
int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_BUILD_CAMERA() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(7)) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true) > StaffSlot(this, 0x15c)) return StaffSlot(this, 0x160);
    return 0;
}

// 0x10012bec
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_GUARD_AREA() { return SetArea(this, 7, 0x170, 0x164, 0x168, 0x16c); }

// 0x10012dd4
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_ENTERTAINER_AREA() { return SetArea(this, 6, 0x188, 0x17c, 0x180, 0x184); }

// 0x10012f18
int32_t CAdvisorScoring::_ADVISOR_STAFF_ENTERTAINER_COVERAGE() {
    if (SessionUnits() < StaffSlot(this, 0x188)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(6)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(6), static_cast<CParkAnalyser::AreaType>(1));
    int32_t result = StaffSlot(this, 0x190) * (100 - percent);
    if (result < StaffSlot(this, 0x18c)) return 0;
    return result;
}

// 0x10013044
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_MECHANICS() { return Train(this, 4, 1, 0x194, 0x198); }

// 0x10013144
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_HANDYMEN() { return Train(this, 5, 0, 0x19c, 0x1a0); }

// 0x10013244
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_GUARDS() { return Train(this, 7, 3, 0x1a4, 0x1a8); }

// 0x10013340
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_ENTERTAINERS() { return Train(this, 6, 2, 0x1ac, 0x1b0); }
