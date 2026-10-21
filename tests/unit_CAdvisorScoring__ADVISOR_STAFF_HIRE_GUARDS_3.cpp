// Test library for seven CAdvisorScoring staff scorers (unit v5265; libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.dylib,
// tests/test_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.py), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The callees outside the unit (CWorld::IsParkClosed / GetParkAnalyser, the CParkAnalyser readers, CParkTime::GetFunnyTimeSinceSessionStart)
// are fakes that record their arguments and answer from the test's queues. gGameWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gGameWorld;

namespace {
CParkAnalyser* gAnalyser;
int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(gGameWorld)); }
}  // namespace

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("closed"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyser;
}

int32_t CParkAnalyser::GetDaysSinceLastStaffHired() const {
    ev("GetDaysSinceLastStaffHired", {lab(this)});
    return static_cast<int32_t>(ret("days"));
}

int32_t CParkAnalyser::GetNumDevelopedCells(uint32_t maxAge) const {
    ev("GetNumDevelopedCells", {lab(this), static_cast<int64_t>(static_cast<int32_t>(maxAge))});
    return static_cast<int32_t>(ret("cells"));
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int64_t>(static_cast<int32_t>(model))});
    return static_cast<int32_t>(ret("staff"));
}

int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    ev("NumberOfStaffTypeWithPatrolArea", {lab(this), static_cast<int64_t>(static_cast<int32_t>(model))});
    return static_cast<int32_t>(ret("patrol"));
}

int32_t CParkAnalyser::MisbehavingVisitorsLastMonth(bool gotAway) const {
    ev("MisbehavingVisitorsLastMonth", {lab(this), static_cast<int64_t>(gotAway ? 1 : 0)});
    return static_cast<int32_t>(ret("mis"));
}

int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessUnder(int32_t level) const {
    ev("PercentageOfVisitorsWithHappinessUnder", {lab(this), static_cast<int64_t>(level)});
    return static_cast<int32_t>(ret("pct"));
}

int32_t CParkAnalyser::HowBusyIsPark() const {
    ev("HowBusyIsPark", {lab(this)});
    return static_cast<int32_t>(ret("busy"));
}

int32_t CParkAnalyser::TypesOfRidesLeftToResearch() const {
    ev("TypesOfRidesLeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("left"));
}

int32_t CParkAnalyser::TypesOfRidesAvailable(bool includeGoldenTickets) const {
    ev("TypesOfRidesAvailable", {lab(this), static_cast<int64_t>(includeGoldenTickets ? 1 : 0)});
    return static_cast<int32_t>(ret("avail"));
}

int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const {
    ev("TypesOfParticularObjectTypeInPark", {lab(this), static_cast<int64_t>(static_cast<int32_t>(type)), static_cast<int64_t>(includeGoldenTickets ? 1 : 0)});
    return static_cast<int32_t>(ret("inpark"));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTimeSinceSessionStart", {Off(this)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(ret("tickhi"))) << 32) | static_cast<uint32_t>(ret("ticklo")));
    return d;
}

extern "C" {

// A zeroed block (32-bit addressable).
uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world and the analyser blocks of the case.
void tpw_adv_setup(uint64_t world, uint64_t analyser) {
    gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
}

// The host address of the balance slot a Mac offset names.
uint64_t tpw_adv_slot(uint64_t object, int32_t macOffset) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&s->mBalance.mValues[(macOffset - 0x14) / 4]));
}

int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

int32_t tpw_adv_call(int32_t which, uint64_t object) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (which) {
        case 0: return s->_ADVISOR_STAFF_HIRE_GUARDS_3();
        case 1: return s->_ADVISOR_STAFF_HIRE_GUARDS_2();
        case 2: return s->_ADVISOR_STAFF_HIRE_ENTERTAINERS_1();
        case 3: return s->_ADVISOR_STAFF_HIRE_GUARDS_1();
        case 4: return s->_ADVISOR_STAFF_HIRE_RESEARCHERS_2();
        case 5: return s->_ADVISOR_STAFF_SET_MECHANIC_AREA();
        default: return s->_ADVISOR_STAFF_SET_HANDYMAN_AREA();
    }
}

}  // extern "C"
