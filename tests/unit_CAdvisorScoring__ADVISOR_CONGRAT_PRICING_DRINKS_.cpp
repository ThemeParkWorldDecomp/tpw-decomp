// Test library for the CONGRAT_PRICING scorers (unit v5122; libtpw_unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS_.dylib), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The analyser and world getter are recording fakes answering from the test's queues ("id", "val").
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
char gWorldCell[16];
char gAnalyserCell[16];
}  // namespace

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserCell);
}

int32_t CParkAnalyser::GetShopWithBestValue(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("BestValueT", {lab(this), static_cast<int64_t>(type)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetShopWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("BestValue", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetShopWithBestSatisfaction(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("BestSatT", {lab(this), static_cast<int64_t>(type)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetShopWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("BestSat", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetSideshowWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("SideshowValue", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

extern "C" {

uint64_t tpw_cps_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldCell},
        {"analyser", gAnalyserCell},
        {"g0", &gAdvisorNoneDrinksValue},
        {"g1", &gAdvisorNoneSideshowsValue},
        {"g2", &gAdvisorNoneRestaurantValue},
        {"g3", &gAdvisorNoneShopsValue},
        {"g4", &gAdvisorNoneFoodSat},
        {"g5", &gAdvisorNoneFoodValue},
        {"g6", &gAdvisorNoneShopsSat},
        {"g7", &gAdvisorNoneRestaurantSat},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// One scorer by index; the limit sits in balance slot `slot`, the multiplier in the next. w4 and w8 go in and come out.
uint32_t tpw_cps_call(int32_t idx, int32_t slot, int32_t limit, int32_t mult, uint32_t* w4, uint32_t* w8) {
    auto* obj = static_cast<CAdvisorScoring*>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)));
    obj->mBalance.mValues[slot].mInteger = limit;
    obj->mBalance.mValues[slot + 1].mInteger = mult;
    obj->mWord4 = static_cast<uint16_t>(*w4);
    obj->mWord8 = *w8;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldCell);
    static int32_t (CAdvisorScoring::*const fns[8])() = {
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_VALUE,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_VALUE,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_VALUE,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_VALUE,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_SATISFACTION,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_VALUE,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_SATISFACTION,
        &CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_SATISFACTION,
    };
    const int32_t r = (obj->*fns[idx])();
    *w4 = obj->mWord4;
    *w8 = obj->mWord8;
    return static_cast<uint32_t>(r);
}

}  // extern "C"
