// Test library for nine advisor scorers (unit v5939; libtpw_unit_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.dylib,
// tests/test_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.py), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The callees outside the unit (the world's getters, the analyser's counts, the research lab, the bank account, the park time) are fakes that
// record the arguments the real ones read and answer from the test's queues. gGameWorld and its reference are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;

namespace {
constexpr uintptr_t kAnalyser = 0x7000, kLab = 0x7100, kBank = 0x7200;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(kAnalyser);
}
CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return reinterpret_cast<CResearchLab*>(kLab);
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(kBank);
}

int32_t CParkAnalyser::TypesOfShopsAvailable(bool unused) const {
    ev("TypesOfShopsAvailable", {lab(this), unused});
    return static_cast<int32_t>(ret("TypesOfShopsAvailable"));
}
int32_t CParkAnalyser::TypesOfSideshowsAvailable(bool unused) const {
    ev("TypesOfSideshowsAvailable", {lab(this), unused});
    return static_cast<int32_t>(ret("TypesOfSideshowsAvailable"));
}
int32_t CParkAnalyser::TypesOfUpgradesAvailable(bool unused) const {
    ev("TypesOfUpgradesAvailable", {lab(this), unused});
    return static_cast<int32_t>(ret("TypesOfUpgradesAvailable"));
}
int32_t CParkAnalyser::TypesOfRidesAvailable(bool golden) const {
    ev("TypesOfRidesAvailable", {lab(this), golden});
    return static_cast<int32_t>(ret("TypesOfRidesAvailable"));
}
int32_t CParkAnalyser::TypesOfRidesLeftToResearch() const {
    ev("TypesOfRidesLeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("TypesOfRidesLeftToResearch"));
}
int32_t CParkAnalyser::TypesOfShopsLeftToResearch() const {
    ev("TypesOfShopsLeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("TypesOfShopsLeftToResearch"));
}
int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const {
    ev("GetDaysSinceLastObjectBuilt", {lab(this)});
    return static_cast<int32_t>(ret("GetDaysSinceLastObjectBuilt"));
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), type, level});
    return static_cast<int32_t>(ret("NumberOfObjectTypeInPark"));
}
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), model});
    return static_cast<int32_t>(ret("NumberOfStaffType"));
}
int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool golden) const {
    ev("TypesOfParticularObjectTypeInPark", {lab(this), type, golden});
    return static_cast<int32_t>(ret("TypesOfParticularObjectTypeInPark"));
}
uint32_t CResearchLab::GetResearchSpeed(ResearchCat category) const {
    ev("GetResearchSpeed", {lab(this), category});
    return static_cast<uint32_t>(ret("GetResearchSpeed"));
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTimeSinceSessionStart", {lab(this)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>(ret("Time"));
    return d;
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offset of balance slot 0 in the scoring object, the stride of a slot and the object's size.
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&o->mBalance.mValues[0]) - reinterpret_cast<char*>(o));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

int32_t tpw_adv_size() { return static_cast<int32_t>(sizeof(CAdvisorScoring)); }

int32_t tpw_adv_call(int32_t which, uint64_t object) {
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (which) {
        case 0: return o->_ADVISOR_RIDES_RESEARCH_MORE();
        case 1: return o->_ADVISOR_RIDES_BUILD_RIDES();
        case 2: return o->_ADVISOR_RIDES_NO_UPGRADES();
        case 3: return o->_ADVISOR_RIDES_MORE_UPGRADES();
        case 4: return o->_ADVISOR_SHOPS_POOR_SELECTION();
        case 5: return o->_ADVISOR_SHOPS_RESEARCH_MORE();
        case 6: return o->_ADVISOR_SHOPS_BUILD_SHOPS();
        case 7: return o->_ADVISOR_SHOPS_BUILD_MORE_SHOPS();
        case 8: return o->_ADVISOR_SIDESHOWS_POOR_SELECTION();
    }
    return 0;
}

}  // extern "C"
