// Test library for eight advisor scorers (unit v5256; libtpw_unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS__2.dylib), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The park analyser, the world, CTRACE, the master save are recording fakes; the world and the analyser are the fixed fake addresses
// 0x1000 and 0x2000 (never dereferenced). gMainBalance is raw storage under the same symbol.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "World.h"
#include "analyser.h"
#include "MainBalance.h"

using namespace calllog;

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;

// ---- globals of other files
CWorld* gGameWorld = reinterpret_cast<CWorld*>(0x1000);
CWorld*& gGameWorldRef = gGameWorld;
CFSGlobalMasterSave* gMasterSave = nullptr;
alignas(16) unsigned char gMainBalanceStore[16384] __asm__("_gMainBalance");

// ---- the fakes
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(0x2000);
}

int32_t CParkAnalyser::GetShopWithBestSatisfaction(CObjectShopType type, ThingId& id) const {
    ev("BestSat", {lab(this), static_cast<int32_t>(type)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetShopWithWorstSatisfaction(CObjectShopType type, ThingId& id) const {
    ev("WorstSatT", {lab(this), static_cast<int32_t>(type)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetShopWithWorstSatisfaction(ThingId& id) const {
    ev("WorstSat", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetSideshowWithWorstSatisfaction(ThingId& id) const {
    ev("SideSat", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::GetSideshowWithWorstValue(ThingId& id) const {
    ev("SideVal", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("id"));
    return static_cast<int32_t>(ret("val"));
}

int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("Visitors", {lab(this)});
    return static_cast<int32_t>(ret("val"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int value = va_arg(args, int);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), value}, {format});
}

// The fake constructor's member clean-up path references this destructor; nothing is ever built, so it is empty here.
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("ctor", {newlab(this)}); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal type) const {
    ev("HaveTicket", {lab(this), static_cast<int32_t>(type)});
    return ret("have") != 0;
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    CMainBalance* main = reinterpret_cast<CMainBalance*>(gMainBalanceStore);
    const Entry entries[] = {
        {"s_sideshows_value", &gAdvisorNoneSideshowsPoorValue},
        {"s_food_sat", &gAdvisorNoneFoodPoorSat},
        {"s_shops_sat", &gAdvisorNoneShopsPoorSat},
        {"s_restaurant_sat", &gAdvisorNoneRestaurantPoorSat},
        {"s_drinks_sat", &gAdvisorNoneDrinksPoorSat},
        {"s_sideshows_sat", &gAdvisorNoneSideshowsPoorSat},
        {"s_drinks_great", &gAdvisorNoneDrinksGreatSat},
        {"gMasterSave", &gMasterSave},
        {"mainValues", &main->mValues[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// HOST_LAYOUT: offset of mValues in the scoring object, sizeof(BalanceValue), offset of mValues in the main table, mWord4, mWord8.
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    unsigned char* block = static_cast<unsigned char*>(tpw::ArenaAllocZeroed(0x2000));
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(block);
    out[n++] = reinterpret_cast<unsigned char*>(&s->mBalance.mValues[0]) - block;
    out[n++] = sizeof(BalanceValue);
    out[n++] = reinterpret_cast<unsigned char*>(&reinterpret_cast<CMainBalance*>(gMainBalanceStore)->mValues[0]) - gMainBalanceStore;
    out[n++] = reinterpret_cast<unsigned char*>(&s->mWord4) - block;
    out[n++] = reinterpret_cast<unsigned char*>(&s->mWord8) - block;
    return n;
}

#define WRAP(NAME) \
    uint32_t tpw_adv_##NAME(uint64_t self) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(self)->_ADVISOR_##NAME()); }
WRAP(CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION)
WRAP(PRICING_SIDESHOWS_POOR_SATISFACTION)
WRAP(PRICING_DRINKS_POOR_SATISFACTION)
WRAP(PRICING_RESTAURANT_POOR_SATISFACTION)
WRAP(GOLD_TICKET_NEAR_TO_XPEEPS)
WRAP(PRICING_SHOPS_POOR_SATISFACTION)
WRAP(PRICING_FOOD_POOR_SATISFACTION)
WRAP(PRICING_SIDESHOWS_POOR_VALUE)

}  // extern "C"
