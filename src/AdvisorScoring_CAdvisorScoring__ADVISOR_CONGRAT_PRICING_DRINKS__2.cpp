// Unit v5256: eight more advisor scorers (pricing satisfaction / value, gold ticket near to X peeps).
// The balance slot of a Mac offset N is (N - 0x14) / 4 (the table's values start at 0x14 in the scoring object).
#include <new>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "MainBalance.h"
#include "World.h"
#include "analyser.h"

// "no thing" ids, zero in the original's data
uint16_t gAdvisorNoneSideshowsPoorValue = 0;       // 0x1027df0e
uint16_t gAdvisorNoneFoodPoorSat = 0;              // 0x1027df10
uint16_t gAdvisorNoneShopsPoorSat = 0;             // 0x1027df12
uint16_t gAdvisorNoneRestaurantPoorSat = 0;        // 0x1027df14
uint16_t gAdvisorNoneDrinksPoorSat = 0;            // 0x1027df16
uint16_t gAdvisorNoneSideshowsPoorSat = 0;         // 0x1027df18
uint16_t gAdvisorNoneDrinksGreatSat = 0;           // 0x1027df2a

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;

int32_t Slot(CAdvisorScoring* self, int32_t slot) { return self->mBalance.mValues[slot].mInteger; }

// The common tail of the POOR scorers: limit at slot, multiplier at slot + 1.
int32_t Poor(CAdvisorScoring* self, int32_t figure, const ThingId& id, uint16_t none, int32_t slot, const char* text) {
    if (id.mIndex == none) return 0;
    if (figure < Slot(self, slot)) {
        CTRACE(0x26, text, figure);
        self->mWord4 = id.mIndex;
        self->mWord8 = static_cast<uint32_t>(figure);
        return Slot(self, slot + 1) * (figure - Slot(self, slot));
    }
    return 0;
}

}  // namespace

// 0x1001a4e8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(4), id);
    if (id.mIndex == gAdvisorNoneDrinksGreatSat) return 0;
    if (figure > Slot(this, 341)) {   // 0x568
        mWord4 = id.mIndex;
        mWord8 = static_cast<uint32_t>(figure);
        return Slot(this, 342) * (figure - Slot(this, 341));   // 0x56c
    }
    return 0;
}

// 0x10019c24
int32_t CAdvisorScoring::_ADVISOR_PRICING_SIDESHOWS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithWorstSatisfaction(id);
    return Poor(this, figure, id, gAdvisorNoneSideshowsPoorSat, 323, "sideshows poor satisfaction (%d)\n");   // 0x520, 0x524
}

// 0x10019b10
int32_t CAdvisorScoring::_ADVISOR_PRICING_DRINKS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(4), id);
    return Poor(this, figure, id, gAdvisorNoneDrinksPoorSat, 321, "drinks poor satisfaction (%d)\n");   // 0x518, 0x51c
}

// 0x100199f8
int32_t CAdvisorScoring::_ADVISOR_PRICING_RESTAURANT_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(3), id);
    return Poor(this, figure, id, gAdvisorNoneRestaurantPoorSat, 319, "restaurant poor satisfaction (%d)\n");   // 0x510, 0x514
}

// 0x1001b054
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XPEEPS() {
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
    // fsubs / fmuls / fctiwz: the balance integer (slot 466 of the main table, Mac +0x750) as a float times the float at 0x5c4
    const float product = static_cast<float>(gMainBalance.mValues[466].mInteger) * mBalance.mValues[364].mFloat;
    if (visitors > static_cast<int32_t>(product)) {
        if (gMasterSave == nullptr) {
            CFSGlobalMasterSave* made = static_cast<CFSGlobalMasterSave*>(::operator new(0x4c));
            if (made != nullptr) new (made) CFSGlobalMasterSave();
            gMasterSave = made;
        }
        if (!gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(0))) return Slot(this, 365);   // 0x5c8
    }
    return 0;
}

// 0x100198ec
int32_t CAdvisorScoring::_ADVISOR_PRICING_SHOPS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(id);
    return Poor(this, figure, id, gAdvisorNoneShopsPoorSat, 317, "shops poor satisfaction (%d)\n");   // 0x508, 0x50c
}

// 0x100197dc
int32_t CAdvisorScoring::_ADVISOR_PRICING_FOOD_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(2), id);
    return Poor(this, figure, id, gAdvisorNoneFoodPoorSat, 315, "Food poor satisfaction (%d)\n");   // 0x500, 0x504
}

// 0x100196d0
int32_t CAdvisorScoring::_ADVISOR_PRICING_SIDESHOWS_POOR_VALUE() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithWorstValue(id);
    return Poor(this, figure, id, gAdvisorNoneSideshowsPoorValue, 313, "sideshows poor value (%d)\n");   // 0x4f8, 0x4fc
}
