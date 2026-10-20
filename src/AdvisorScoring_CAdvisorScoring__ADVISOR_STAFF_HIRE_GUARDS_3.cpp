// Unit v5265: seven staff scorers of CAdvisorScoring (guards 1..3 hiring, entertainer hiring, researcher hiring, mechanic and handyman
// patrol areas). Balance values are read by their Mac offset from the scoring object (slot 0 is at 0x14).
#include <cstdint>

#include "AdvisorScoring.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "analyser.h"

namespace {

inline CParkAnalyser* Analyser() { return gGameWorld->GetParkAnalyser(); }
inline CThing::ThingModel Model(int32_t m) { return static_cast<CThing::ThingModel>(m); }

}  // namespace

#define ADV_BAL(off) (mBalance.mValues[((off) - 0x14) / 4].mInteger)

// 0x10011d4c: hire guards when the park is open, the last hire was long enough ago, there is no guard and the park is busy.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->GetDaysSinceLastStaffHired() < ADV_BAL(0x630)) return 0;
    if (Analyser()->NumberOfStaffType(Model(7)) > 0) return 0;
    int32_t busy = Analyser()->HowBusyIsPark();
    return ADV_BAL(0xf4) * (busy - ADV_BAL(0xf0));
}

// 0x10011e64: no guard yet: a base plus a weight per misbehaving visitor last month.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model(7)) > 0) return 0;
    int32_t bad = Analyser()->MisbehavingVisitorsLastMonth(false);
    return ADV_BAL(0xf8) + ADV_BAL(0xfc) * bad;
}

// 0x10011f54: hire more guards by the developed cells left uncovered.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_3() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->GetDaysSinceLastStaffHired() < ADV_BAL(0x630)) return 0;
    int32_t cells = Analyser()->GetNumDevelopedCells(0);
    if (cells < ADV_BAL(0x108)) return 0;
    int32_t guards = Analyser()->NumberOfStaffType(Model(7));
    int32_t left = cells - ADV_BAL(0x100) * guards;
    if (left <= 0) return 0;
    return left * ADV_BAL(0x104);
}

// 0x10012090
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_ENTERTAINERS_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model(6)) > 0) return 0;
    if (Analyser()->PercentageOfVisitorsWithHappinessUnder(30) <= 30) return 0;
    int32_t busy = Analyser()->HowBusyIsPark();
    return ADV_BAL(0x110) * (busy - ADV_BAL(0x10c));
}

// 0x10012328: no researcher, rides left to research, and every available ride type is in the park.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model(8)) > 0) return 0;
    if (Analyser()->TypesOfRidesLeftToResearch() == 0) return 0;
    int32_t available = Analyser()->TypesOfRidesAvailable(false);
    int32_t inPark = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    return inPark == available ? ADV_BAL(0x11c) : 0;
}

namespace {

// The funny time since the session start, in units of 0x1792f8648000 ticks (the 64-bit divide at 0x10237e1c), low word.
inline int32_t FunnyPeriods() {
    const CParkTime* parkTime = reinterpret_cast<const CParkTime*>(reinterpret_cast<const uint8_t*>(gGameWorld) + 0x2a0);
    TbTimeDiff d = parkTime->GetFunnyTimeSinceSessionStart();
    return static_cast<int32_t>(d.mTicks / 0x1792f8648000LL);
}

}  // namespace

// 0x1001260c
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_MECHANIC_AREA() {
    if (FunnyPeriods() < ADV_BAL(0x138)) return 0;
    int32_t staff = Analyser()->NumberOfStaffType(Model(4));
    if (staff < ADV_BAL(0x12c)) return 0;
    int32_t percent = Analyser()->NumberOfStaffTypeWithPatrolArea(Model(4)) * 100 / staff;
    if (percent > ADV_BAL(0x130)) return 0;
    return (100 - percent) * ADV_BAL(0x134);
}

// 0x100128a4
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_HANDYMAN_AREA() {
    if (FunnyPeriods() < ADV_BAL(0x150)) return 0;
    int32_t staff = Analyser()->NumberOfStaffType(Model(5));
    if (staff < ADV_BAL(0x144)) return 0;
    int32_t percent = Analyser()->NumberOfStaffTypeWithPatrolArea(Model(5)) * 100 / staff;
    if (percent > ADV_BAL(0x148)) return 0;
    return (100 - percent) * ADV_BAL(0x14c);
}
