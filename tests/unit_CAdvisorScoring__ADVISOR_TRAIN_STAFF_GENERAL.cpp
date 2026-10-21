// Test library for eight advisor scorers (unit v5282; libtpw_unit_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.dylib,
// tests/test_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.py), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The callees outside the unit (world getters, analyser counts, the worst value query, the park time, CTRACE) are fakes that record the
// arguments the real ones read and answer from the test's queues. gGameWorld and gGameWorldRef are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;

namespace {
CParkAnalyser* gFakeAnalyser = nullptr;
CStaffHQ* gFakeHq = nullptr;
}  // namespace

// ---- the fakes
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gFakeAnalyser;
}

CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return gFakeHq;
}

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}

int32_t CParkAnalyser::NumberOfStaff() const {
    ev("NumberOfStaff", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfStaff"));
}

int32_t CParkAnalyser::NumberOfStaffTooTiredToWork() const {
    ev("Tired", {lab(this)});
    return static_cast<int32_t>(ret("Tired"));
}

int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    ev("Patrol", {lab(this), static_cast<int64_t>(static_cast<int32_t>(model))});
    return static_cast<int32_t>(ret("Patrol"));
}

int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("Visitors", {lab(this)});
    return static_cast<int32_t>(ret("Visitors"));
}

int32_t CParkAnalyser::NumberOfVisitorsWithCostumes() const {
    ev("Costumes", {lab(this)});
    return static_cast<int32_t>(ret("Costumes"));
}

int32_t CParkAnalyser::GetShopWithWorstValue(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("WorstT", {lab(this), static_cast<int64_t>(static_cast<int32_t>(type))});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("WorstT"));
}

int32_t CParkAnalyser::GetShopWithWorstValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("Worst", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("Worst"));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("Funny", {lab(this)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(ret("FunnyHi")) << 32) | (static_cast<uint64_t>(ret("FunnyLo")) & 0xffffffffu));
    return d;
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(value)}, {format});
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"none_food", &gAdvisorNoneFoodPoorValue},
        {"none_shops", &gAdvisorNoneShopsPoorValue},
        {"none_rest", &gAdvisorNoneRestaurantPoorValue},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_adv_install(uint64_t world, uint64_t analyser, uint64_t hq) {
    gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gFakeAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    gFakeHq = reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq));
}

// The host address of the balance value at a Mac offset of the scoring object.
uint64_t tpw_adv_slot(uint64_t object, int32_t macOffset) {
    CAdvisorScoring* self = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&self->mBalance.mValues[(macOffset - 0x14) / 4]));
}

int32_t tpw_adv_layout(int64_t* out) {
    char* block = static_cast<char*>(tpw::ArenaAllocZeroed(0x4000));
    CAdvisorScoring* self = reinterpret_cast<CAdvisorScoring*>(block);
    CWorld* world = reinterpret_cast<CWorld*>(block);
    CStaffHQ* hq = reinterpret_cast<CStaffHQ*>(block);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&self->mWord4) - block;
    out[n++] = reinterpret_cast<char*>(&self->mWord8) - block;
    out[n++] = reinterpret_cast<char*>(&world->mParkTime) - block;
    out[n++] = reinterpret_cast<char*>(&hq->mTraining.mF14) - block;
    return n;
}

#define SCORER(name) \
    uint32_t tpw_adv##name(uint64_t object) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object))->name()); }
SCORER(_ADVISOR_TRAIN_STAFF_GENERAL)
SCORER(_ADVISOR_SET_PATROLS_GENERAL)
SCORER(_ADVISOR_HIRE_STAFF_GENERAL)
SCORER(_ADVISOR_NEED_STAFF_ROOM)
SCORER(_ADVISOR_PRICING_FOOD_POOR_VALUE)
SCORER(_ADVISOR_PRICING_SHOPS_POOR_VALUE)
SCORER(_ADVISOR_PRICING_RESTAURANT_POOR_VALUE)
SCORER(_ADVISOR_CONGRAT_COSTUMES)

}  // extern "C"
