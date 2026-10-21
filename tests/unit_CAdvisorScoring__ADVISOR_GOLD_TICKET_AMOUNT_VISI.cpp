// Test library for the advisor's gold ticket scorers and the tooltip tutorial scorer (unit v5106;
// libtpw_unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.dylib, tests/test_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.py), main app:
//   src/game/advisor/AdvisorScoring.cpp   the eight scorers
//
// The callees outside the unit are recording fakes: CWorld::IsParkClosed, CWorld::GetParkAnalyser, CParkAnalyser::NumberOfVisitorsInPark,
// CParkAnalyser::PercentageOfTotalObjectsInPark, CParkTime::GetFunnyTimeSinceSessionStart and CHistory<int,5>::HowFarBack. The world and the
// analyser are constant addresses that are never dereferenced; gMainBalance is raw storage (its constructor is not wanted).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "History.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

namespace {
constexpr uintptr_t kWorld = 0x20000000u;
constexpr uintptr_t kAnalyser = 0x20100000u;
constexpr int32_t kSlotBase = 0x14;
}  // namespace

// ---- globals of other files, defined here
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;
alignas(16) char gMainBalanceStorage[8192] __asm__("_gMainBalance");   // the symbol of `extern CMainBalance gMainBalance`, as zeroed storage

// ---- the fakes
int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(const_cast<CWorld*>(this))});
    return static_cast<int32_t>(ret("IsParkClosed"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(kAnalyser);
}

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(const_cast<CParkAnalyser*>(this))});
    return static_cast<int32_t>(ret("NumberOfVisitorsInPark"));
}

int32_t CParkAnalyser::PercentageOfTotalObjectsInPark() const {
    ev("PercentageOfTotalObjectsInPark", {lab(const_cast<CParkAnalyser*>(this))});
    return static_cast<int32_t>(ret("PercentageOfTotalObjectsInPark"));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTimeSinceSessionStart", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(kWorld))});
    TbTimeDiff d;
    d.mTicks = ret("GetFunnyTimeSinceSessionStart");
    return d;
}

template <>
int32_t CHistory<int, 5>::HowFarBack() const {
    const char* expected = reinterpret_cast<const char*>(&reinterpret_cast<CParkAnalyser*>(kAnalyser)->mHistory216D8);
    ev("HowFarBack", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - expected)});
    return static_cast<int32_t>(ret("HowFarBack"));
}

extern "C" {

uint64_t tpw_gt_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return kWorld;
    if (std::strcmp(name, "analyser") == 0) return kAnalyser;
    if (std::strcmp(name, "gGameWorld") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    if (std::strcmp(name, "mb754") == 0)
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMainBalance.mValues[(0x754 - 8) / 4].mInteger));
    return 0;
}

uint64_t tpw_gt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A scoring object (zeroed) and a write of the slot at a Mac offset.
uint64_t tpw_gt_object() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)))); }

void tpw_gt_slot(uint64_t object, int32_t macOffset, int32_t value) {
    reinterpret_cast<CAdvisorScoring*>(object)->mBalance.mValues[(macOffset - kSlotBase) / 4].mInteger = value;
}

int32_t tpw_gt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = reinterpret_cast<const char*>(&reinterpret_cast<CParkAnalyser*>(kAnalyser)->mHistory216D8) - reinterpret_cast<const char*>(kAnalyser);
    out[n++] = reinterpret_cast<const char*>(&reinterpret_cast<CWorld*>(kWorld)->mParkTime) - reinterpret_cast<const char*>(kWorld);
    return n;
}

uint32_t tpw_gt_run(int32_t which, uint64_t object) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(object);
    switch (which) {
        case 0: return static_cast<uint32_t>(s->_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS());
        case 1: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_EXPLANATION());
        case 2: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a());
        case 3: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b());
        case 4: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c());
        case 5: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a());
        case 6: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b());
        default: return static_cast<uint32_t>(s->_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c());
    }
}

}  // extern "C"
