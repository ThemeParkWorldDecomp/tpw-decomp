// Test library for the advisor's staff hire, hungry and thirsty scorers (unit v5136;
// libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3.dylib), main app:
//   src/AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3.cpp
//
// The callees outside the unit (CWorld::GetParkAnalyser, IsParkClosed and the CParkAnalyser counters) are recording fakes that answer from
// the test's queues. gGameWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;

namespace {
CParkAnalyser* gAnalyser = nullptr;
}

// ---- the fakes
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyser;
}

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}

int32_t CParkAnalyser::GetDaysSinceLastStaffHired() const {
    ev("GetDaysSinceLastStaffHired", {lab(this)});
    return static_cast<int32_t>(ret("GetDaysSinceLastStaffHired"));
}

int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), static_cast<int32_t>(type), level});
    return static_cast<int32_t>(ret("NumberOfObjectTypeInPark"));
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("NumberOfStaffType"));
}

int32_t CParkAnalyser::NumberOfBrokenDownRidesInPark() const {
    ev("NumberOfBrokenDownRidesInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfBrokenDownRidesInPark"));
}

int32_t CParkAnalyser::NumberOfWornRidesInPark(int32_t level) const {
    ev("NumberOfWornRidesInPark", {lab(this), level});
    return static_cast<int32_t>(ret("NumberOfWornRidesInPark"));
}

int32_t CParkAnalyser::GetPercentageLitteredCells() const {
    ev("GetPercentageLitteredCells", {lab(this)});
    return static_cast<int32_t>(ret("GetPercentageLitteredCells"));
}

int32_t CParkAnalyser::GetNumDevelopedCells(uint32_t maxAge) const {
    ev("GetNumDevelopedCells", {lab(this), static_cast<int64_t>(maxAge)});
    return static_cast<int32_t>(ret("GetNumDevelopedCells"));
}

int32_t CParkAnalyser::NumberOfHungryVisitors(uint8_t level) const {
    ev("NumberOfHungryVisitors", {lab(this), level});
    return static_cast<int32_t>(ret("NumberOfHungryVisitors"));
}

int32_t CParkAnalyser::NumberOfThirstyVisitors(uint8_t level) const {
    ev("NumberOfThirstyVisitors", {lab(this), level});
    return static_cast<int32_t>(ret("NumberOfThirstyVisitors"));
}

extern "C" {

uint64_t tpw_ass_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world and analyser the fakes work with.
void tpw_ass_setup(uint64_t world, uint64_t analyser) {
    gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
}

// [offset of the balance slot 0 in the scoring object, sizeof(CAdvisorScoring), sizeof(BalanceValue)]
int32_t tpw_ass_layout(int64_t* out) {
    char* block = static_cast<char*>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring) + 16));
    CAdvisorScoring* object = reinterpret_cast<CAdvisorScoring*>(block);
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&object->mBalance.mValues[0]) - block);
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

uint32_t tpw_ass_thirsty(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_VISITORS_THIRSTY()); }
uint32_t tpw_ass_hungry(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_VISITORS_HUNGRY()); }
uint32_t tpw_ass_mech1(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_MECHANICS_1()); }
uint32_t tpw_ass_mech2(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_MECHANICS_2()); }
uint32_t tpw_ass_mech3(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_MECHANICS_3()); }
uint32_t tpw_ass_hand1(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_HANDYMEN_1()); }
uint32_t tpw_ass_hand2(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_HANDYMEN_2()); }
uint32_t tpw_ass_hand3(uint64_t o) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(o)->_ADVISOR_STAFF_HIRE_HANDYMEN_3()); }

}  // extern "C"
