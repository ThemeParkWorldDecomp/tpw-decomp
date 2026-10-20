// Test library for six advisor scorers (unit v5081; libtpw_unit_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.dylib,
// tests/test_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.py), main app:
//   src/AdvisorScoring_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.cpp
//
// The callees (world, analyser, bank account) are fakes that record the arguments and answer from the test's queues; the world pointer
// the scorers read is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

namespace {
alignas(16) char gFakeWorld[16];
alignas(16) char gFakeAnalyser[16];
alignas(16) char gFakeBank[16];
}  // namespace

CWorld* gGameWorld = reinterpret_cast<CWorld*>(gFakeWorld);
CWorld*& gGameWorldRef = gGameWorld;

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gFakeAnalyser);
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gFakeBank);
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("NumberOfStaffType"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CParkAnalyser::FilthiestToilet(int32_t& filth) const {
    ev("FilthiestToilet", {lab(this)});
    filth = static_cast<int32_t>(ret("Filth"));
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("FilthId"));
    return id;
}

int32_t CParkAnalyser::AvgFilthLevelOfToilets() const {
    ev("AvgFilthLevelOfToilets", {lab(this)});
    return static_cast<int32_t>(ret("AvgFilthLevelOfToilets"));
}

int32_t CParkAnalyser::TypesOfFeaturesAvailable(bool unused) const {
    ev("TypesOfFeaturesAvailable", {lab(this), unused ? 1 : 0});
    return static_cast<int32_t>(ret("TypesOfFeaturesAvailable"));
}

int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const {
    ev("GetDaysSinceLastObjectBuilt", {lab(this)});
    return static_cast<int32_t>(ret("GetDaysSinceLastObjectBuilt"));
}

int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), static_cast<int32_t>(type), level});
    return static_cast<int32_t>(ret("NumberOfObjectTypeInPark"));
}

int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const {
    ev("TypesOfParticularObjectTypeInPark", {lab(this), static_cast<int32_t>(type), includeGoldenTickets ? 1 : 0});
    return static_cast<int32_t>(ret("TypesOfParticularObjectTypeInPark", static_cast<int64_t>(type), 0));
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gFakeWorld},
        {"analyser", gFakeAnalyser},
        {"bank", gFakeBank},
        {"id1", &gAdvisorNoToilet1},
        {"id2", &gAdvisorNoToilet2},
        {"id3", &gAdvisorNoToilet3},
        {"id4", &gAdvisorNoToilet4},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offset of mValues[0] in the object, the offset of mWord4, the size of a slot, the size of the object.
int32_t tpw_adv_layout(int64_t* out) {
    CAdvisorScoring& probe = *static_cast<CAdvisorScoring*>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)));
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&probe.mBalance.mValues[0]) - reinterpret_cast<char*>(&probe);
    out[n++] = reinterpret_cast<char*>(&probe.mWord4) - reinterpret_cast<char*>(&probe);
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    return n;
}

int32_t tpw_adv_call(int32_t which, uint64_t object) {
    CAdvisorScoring* self = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (which) {
        case 1: return self->_ADVISOR_FEATURES_DIRTY_TOILETS_1();
        case 2: return self->_ADVISOR_FEATURES_DIRTY_TOILETS_2();
        case 3: return self->_ADVISOR_FEATURES_DIRTY_TOILETS_3();
        case 4: return self->_ADVISOR_FEATURES_DIRTY_TOILETS_4();
        case 5: return self->_ADVISOR_FEATURES_POOR_SELECTION();
        default: return self->_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS();
    }
}

}  // extern "C"
