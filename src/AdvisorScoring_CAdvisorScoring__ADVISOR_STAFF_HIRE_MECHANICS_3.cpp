// AdvisorScoring (main app): the staff hire, hungry and thirsty visitor scorers (unit v5136).
//   _ADVISOR_VISITORS_THIRSTY 0x10011640, _ADVISOR_VISITORS_HUNGRY 0x100116d8, _ADVISOR_STAFF_HIRE_MECHANICS_1 0x1001176c,
//   _ADVISOR_STAFF_HIRE_MECHANICS_2 0x10011838, _ADVISOR_STAFF_HIRE_MECHANICS_3 0x10011900, _ADVISOR_STAFF_HIRE_HANDYMEN_1 0x10011a08,
//   _ADVISOR_STAFF_HIRE_HANDYMEN_2 0x10011b0c, _ADVISOR_STAFF_HIRE_HANDYMEN_3 0x10011c0c.
// The balance values are read at Mac offsets of the scoring object: the table's values start at 0x18 (slot 0), four bytes a slot.
#include <cstdint>

#include "AdvisorScoring.h"
#include "World.h"
#include "analyser.h"

namespace {

// The balance value the Mac reads at `macOffset` of the scoring object.
inline int32_t Bal(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x18) / 4].mInteger;
}

inline CParkAnalyser* Analyser() { return gGameWorld->GetParkAnalyser(); }

const CThing::ThingModel kMechanic = static_cast<CThing::ThingModel>(4);
const CThing::ThingModel kHandyman = static_cast<CThing::ThingModel>(5);

}  // namespace

int32_t CAdvisorScoring::_ADVISOR_VISITORS_THIRSTY() {
    int32_t count = Analyser()->NumberOfThirstyVisitors(static_cast<uint8_t>(Bal(this, 0xb0)));
    return Bal(this, 0xac) * count;
}

int32_t CAdvisorScoring::_ADVISOR_VISITORS_HUNGRY() {
    int32_t count = Analyser()->NumberOfHungryVisitors(static_cast<uint8_t>(Bal(this, 0xb8)));
    return Bal(this, 0xb4) * count;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_1() {
    if (Analyser()->NumberOfStaffType(kMechanic) > 0) return 0;
    int32_t worn = Analyser()->NumberOfWornRidesInPark(Bal(this, 0xc0));
    return Bal(this, 0xbc) * worn;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_2() {
    if (Analyser()->NumberOfStaffType(kMechanic) > 0) return 0;
    int32_t broken = Analyser()->NumberOfBrokenDownRidesInPark();
    return Bal(this, 0xc4) * broken;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_3() {
    if (Analyser()->GetDaysSinceLastStaffHired() < Bal(this, 0x630)) return 0;
    int32_t rides = Analyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    int32_t surplus = rides - Bal(this, 0xc8) * Analyser()->NumberOfStaffType(kMechanic);
    if (surplus <= 0) return 0;
    return surplus * Bal(this, 0xcc);
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) > 0) return 0;
    int32_t littered = Analyser()->GetPercentageLitteredCells();
    if (littered > Bal(this, 0xd8)) return 0;
    return Bal(this, 0xd0) * littered - Bal(this, 0xd4);
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) > 0) return 0;
    int32_t littered = Analyser()->GetPercentageLitteredCells();
    int32_t limit = Bal(this, 0xe0);
    if (littered < limit) return 0;
    return Bal(this, 0xdc) * littered - limit;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_3() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) == 0) return 0;
    int32_t cells = Analyser()->GetNumDevelopedCells(0);
    if (cells < Bal(this, 0xec)) return 0;
    int32_t surplus = cells - Bal(this, 0xe4) * Analyser()->NumberOfStaffType(kHandyman);
    if (surplus <= 0) return 0;
    return surplus * Bal(this, 0xe8);
}
