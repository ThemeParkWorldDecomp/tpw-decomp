// Unit v5122: the CONGRAT_PRICING great value / great satisfaction scorers (main app). Each asks the park analyser for its best thing,
// and when that is not the "none" id and the figure beats the balance limit, records it in the object (halfword 4, word 8) and answers
// multiplier * (figure - limit).
#include <cstdint>

#include "AdvisorScoring.h"
#include "World.h"
#include "analyser.h"

uint16_t gAdvisorNoneFoodValue = 0;         // 0x1027df1a
uint16_t gAdvisorNoneShopsValue = 0;        // 0x1027df1c
uint16_t gAdvisorNoneRestaurantValue = 0;   // 0x1027df1e
uint16_t gAdvisorNoneDrinksValue = 0;       // 0x1027df20
uint16_t gAdvisorNoneSideshowsValue = 0;    // 0x1027df22
uint16_t gAdvisorNoneFoodSat = 0;           // 0x1027df24
uint16_t gAdvisorNoneShopsSat = 0;          // 0x1027df26
uint16_t gAdvisorNoneRestaurantSat = 0;     // 0x1027df28

namespace {

// The balance value at a Mac offset of the scoring object (the values start at 0x18 there, four bytes each).
int32_t Slot(const CAdvisorScoring* self, int32_t macOffset) { return self->mBalance.mValues[(macOffset - 0x18) / 4].mInteger; }

}  // namespace

#define TPW_CONGRAT_FINISH(NONE, OFFSET)                      \
    if (id.mIndex == (NONE)) return 0;                        \
    if (figure > Slot(this, OFFSET)) {                        \
        mWord4 = id.mIndex;                                   \
        mWord8 = static_cast<uint32_t>(figure);               \
        return Slot(this, (OFFSET) + 4) * (figure - Slot(this, OFFSET)); \
    }                                                         \
    return 0;

// 0x1001a010
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(4), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneDrinksValue, 0x540)
}

// 0x1001a104
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithBestValue(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneSideshowsValue, 0x548)
}

// 0x10019f18
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(3), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneRestaurantValue, 0x538)
}

// 0x10019e28
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneShopsValue, 0x530)
}

// 0x1001a1f8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(2), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneFoodSat, 0x550)
}

// 0x10019d34
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(2), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneFoodValue, 0x528)
}

// 0x1001a2f0
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneShopsSat, 0x558)
}

// 0x1001a3e8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(3), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneRestaurantSat, 0x560)
}
