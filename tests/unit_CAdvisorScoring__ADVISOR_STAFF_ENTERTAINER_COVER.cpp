// Test library for the advisor's staff scorers (unit v5118; libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.dylib,
// tests/test_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.py), main app:
//   src/AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.cpp   the eight _ADVISOR_STAFF_* scorers
//
// The callees outside the unit (the world's getters, the analyser's staff queries, the session time) are fakes that record the arguments the
// real ones read and answer from the test's queues. gGameWorld and gGameWorldRef are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "ThingParts.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;

namespace {
CParkAnalyser* gTestAnalyser;
CStaffHQ* gTestHq;
}  // namespace

// ---- the fakes
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("FunnyTime", {lab(this)});
    uint64_t hi = static_cast<uint64_t>(ret("ticksHi")) & 0xffffffffu;
    uint64_t lo = static_cast<uint64_t>(ret("ticksLo")) & 0xffffffffu;
    TbTimeDiff t;
    t.mTicks = static_cast<int64_t>((hi << 32) | lo);
    return t;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gTestAnalyser;
}

CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return gTestHq;
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("NumberOfStaffType"));
}

int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    ev("NumberOfStaffTypeWithPatrolArea", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("NumberOfStaffTypeWithPatrolArea"));
}

int32_t CParkAnalyser::PercentageOfAreaPatrolledBy(CThing::ThingModel model, AreaType area) {
    ev("PercentageOfAreaPatrolledBy", {lab(this), static_cast<int32_t>(model), static_cast<int32_t>(area)});
    return static_cast<int32_t>(ret("PercentageOfAreaPatrolledBy"));
}

int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    ev("AverageQualityOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("AverageQualityOfStaffType"));
}

int32_t CParkAnalyser::MisbehavingVisitorsLastMonth(bool gotAway) const {
    ev("MisbehavingVisitorsLastMonth", {lab(this), gotAway ? 1 : 0});
    return static_cast<int32_t>(ret("MisbehavingVisitorsLastMonth"));
}

extern "C" {

uint64_t tpw_stf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gameworld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_stf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_stf_setptrs(uint64_t analyser, uint64_t hq) {
    gTestAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    gTestHq = reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq));
}

// [byte offset of balance value slot 0 in the scoring object, byte offset of the training words in the headquarters]
int32_t tpw_stf_layout(int64_t* out) {
    alignas(16) static char scoringBuf[sizeof(CAdvisorScoring)];
    alignas(16) static char hqBuf[sizeof(CStaffHQ)];
    auto* s = reinterpret_cast<CAdvisorScoring*>(scoringBuf);
    auto* h = reinterpret_cast<CStaffHQ*>(hqBuf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&s->mBalance.mValues[0]) - scoringBuf;
    out[n++] = reinterpret_cast<char*>(&h->mTraining.mF00[0]) - hqBuf;
    return n;
}

int32_t tpw_stf_0(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_ENTERTAINER_COVERAGE(); }
int32_t tpw_stf_1(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_TRAIN_MECHANICS(); }
int32_t tpw_stf_2(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_SET_ENTERTAINER_AREA(); }
int32_t tpw_stf_3(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_TRAIN_HANDYMEN(); }
int32_t tpw_stf_4(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_SET_GUARD_AREA(); }
int32_t tpw_stf_5(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_TRAIN_GUARDS(); }
int32_t tpw_stf_6(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_GUARD_BUILD_CAMERA(); }
int32_t tpw_stf_7(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->_ADVISOR_STAFF_TRAIN_ENTERTAINERS(); }

}  // extern "C"
