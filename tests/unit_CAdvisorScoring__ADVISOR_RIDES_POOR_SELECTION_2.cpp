// Test library for six advisor scorers (unit v5973; libtpw_unit_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2.dylib), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The analyser, research lab, park time, history and map callees are recording fakes answering from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "AdvisorScoring.h"
#include "History.h"
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
CParkAnalyser* sAnalyser = nullptr;
CResearchLab* sLab = nullptr;
}  // namespace

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return sAnalyser;
}
CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return sLab;
}
int32_t CParkAnalyser::TypesOfRidesAvailable(bool flag) const {
    ev("TypesOfRidesAvailable", {lab(this), flag});
    return static_cast<int32_t>(ret("RidesAvail"));
}
int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const {
    ev("GetDaysSinceLastObjectBuilt", {lab(this)});
    return static_cast<int32_t>(ret("Days"));
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), type, level});
    return static_cast<int32_t>(ret("NumObj"));
}
int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool flag) const {
    ev("TypesOfParticularObjectTypeInPark", {lab(this), type, flag});
    return static_cast<int32_t>(ret("TP", type, 0));
}
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int64_t>(model)});
    return static_cast<int32_t>(ret("Staff"));
}
int32_t CParkAnalyser::TypesOfSideshowsLeftToResearch() const {
    ev("TypesOfSideshowsLeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("ShowsLeft"));
}
int32_t CParkAnalyser::TypesOfSideshowsAvailable(bool flag) const {
    ev("TypesOfSideshowsAvailable", {lab(this), flag});
    return static_cast<int32_t>(ret("ShowsAvail"));
}
int32_t CParkAnalyser::TypesOfFeaturesLeftToResearch() const {
    ev("TypesOfFeaturesLeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("FeatLeft"));
}
int32_t CParkAnalyser::TypesOfFeaturesAvailable(bool flag) const {
    ev("TypesOfFeaturesAvailable", {lab(this), flag});
    return static_cast<int32_t>(ret("FeatAvail"));
}
uint32_t CResearchLab::GetResearchSpeed(ResearchCat category) const {
    ev("GetResearchSpeed", {lab(this), static_cast<int64_t>(category)});
    return static_cast<uint32_t>(ret("Speed"));
}
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTime", {lab(this)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(ret("TicksHi"))) << 32) | static_cast<uint32_t>(ret("TicksLo")));
    return d;
}
template <>
int32_t CHistory<int, 5>::HowFarBack() const {
    ev("HowFarBack", {lab(this)});
    return static_cast<int32_t>(ret("HowFar"));
}
template <>
bool CHistory<int, 5>::GetValue(int& out, int32_t back) const {
    ev("GetValue", {lab(this), back});
    out = static_cast<int32_t>(ret("GV", back, 0));
    return ret("GVok", back, 0) & 1;
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetBusStopPos(int32_t which) const {
    ev("GetBusStopPos", {lab(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("BusPos"));
    return t;
}

extern "C" {

uint64_t tpw_v5973_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_v5973_newthis() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)))); }
void tpw_v5973_setup(uint64_t world, uint64_t analyser, uint64_t lab_) {
    (void)world;
    sAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    sLab = reinterpret_cast<CResearchLab*>(static_cast<uintptr_t>(lab_));
}
void tpw_v5973_setworld(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }
void tpw_v5973_setbal(uint64_t self, int32_t macOffset, int32_t value) {
    reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->mBalance.mValues[(macOffset - 0x14) / 4].mInteger = value;
}
uint32_t tpw_v5973_word8(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->mWord8; }

int32_t tpw_v5973_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory216D8));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    return n;
}

#define W(NAME, FN) \
    uint32_t tpw_v5973_##NAME(uint64_t self) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(self))->FN()); }
W(rides, _ADVISOR_RIDES_POOR_SELECTION)
W(sres, _ADVISOR_SIDESHOWS_RESEARCH_MORE)
W(sbuild, _ADVISOR_SIDESHOWS_BUILD_SIDESHOWS)
W(fres, _ADVISOR_FEATURES_RESEARCH_MORE)
W(hype, _ADVISOR_HYPE_NEWPARK)
W(bus, _ADVISOR_BIG_BUSLOAD_ARRIVED)

}  // extern "C"
