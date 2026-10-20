// The park analyser's shop and sideshow superlatives (main app, analyser.cpp range; unit w152): 12 functions, 0x100eaa24 to 0x100ec228 (5,352 bytes).
// CParkAnalyser::GetShopWith{Best,Worst}{Value,Satisfaction} (twice: with a CObject::ShopType, and without one for the food and drink types 1, 5 and 6)
// and GetSideshowWith{Best,Worst}{Value,Satisfaction}.
//
// All twelve are one walk: the used-thing walk of HowBusyIsPark (src/ParkAnalyser_readers.cpp), after clearing the caller's result halfword. Per thing
// the test, in the listing's order, each step short-circuiting:
//  1. the kind byte is 3 (an object);
//  2. the filter: the ride record's ShopType slot (+0x164, signed) equals the type (GetDBInfo called once), or is 1, 5 or 6 (up to three separate
//     GetDBInfo calls), or for the sideshows the ride database's record of mObjectType has mUIType (+0x7a8) 2;
//  3. GetObjectAge() > 30 (signed);
//  4. the VALUE variants: the sum of the last 30 of mHistory1A4 as a double, converted to unsigned (__cvt_fp2unsigned); 0 skips the thing; then
//     (converted sum of mHistory22C * 100) / (converted sum of mHistory1A4, summed a third time), unsigned, then compared as a signed word. The listing
//     converts each 1A4 sum twice in a row (the same double, a pure helper): one call here. The SATISFACTION variants: GetCustomerSatisfaction()
//     (a signed word); the sideshow BestSatisfaction first needs the converted 1A4 sum to be nonzero, as the value variants do;
//  5. keep: Best when nothing was kept yet or the new value is strictly greater, Worst when strictly smaller (signed compares).
// The answer is the kept value, 0 when nothing matched. The end id of the walk is a halfword of initialised data (a different one per function), all 0.
//
// QUIRK: GetSideshowWithWorstSatisfaction 0x100ec228 is the same code as GetSideshowWithWorstValue (it answers the value ratio, an original copy-paste).
#include <cstdint>

#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

constexpr uint16_t kNoThing = 0;
constexpr uint8_t kObjectKind = 3;
constexpr int32_t kMinimumAge = 30;
constexpr int32_t kSideshowUIType = 2;
constexpr uint32_t kHistoryCount = 30;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240129 + 0x119 = 0x10240242

using ThingIdRef = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>&;

enum class Pick { kBest, kWorst };

const CWorldSavvy* Savvy(const CParkAnalyser* analyser) { return reinterpret_cast<const CWorldSavvy*>(analyser); }

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The ride record's ShopType slot, read through GetDBInfo (one call per read).
int32_t ShopTypeOf(const CObject* object) { return object->GetDBInfo()->Value(kRideSlot_ShopType).mInteger; }

// The filter of the shops of one type.
struct OfType {
    int32_t type;
    bool operator()(const CObject* object) const { return type == ShopTypeOf(object); }
};

// The filter of the food and drink shops: the slot is 1, 5 or 6, each compare reads it again through its own GetDBInfo.
struct AnyShop {
    bool operator()(const CObject* object) const { return ShopTypeOf(object) == 1 || ShopTypeOf(object) == 5 || ShopTypeOf(object) == 6; }
};

// The filter of the sideshows: the ride database's record of the object type has UI type 2.
struct Sideshow {
    bool operator()(const CObject* object) const {
        return gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType))->mUIType == kSideshowUIType;
    }
};

// The converted sum of the last 30 of a history (GetSumOfLast, then __cvt_fp2unsigned).
uint32_t SumOf(const CHistory<uint32_t, 30>& history) {
    double sum;
    history.GetSumOfLast(sum, kHistoryCount);
    return cw_cvt_fp2unsigned(sum);
}

// The value variants' number: nothing (skip the thing) when the converted 1A4 sum is 0, else a * 100 / b, unsigned, as a signed word.
// The listing converts the first 1A4 sum twice in a row (one pure call here), and sums the 1A4 history a second time for the divisor.
bool ValueOf(const CObject* object, int32_t& value) {
    if (SumOf(object->mHistory1A4) == 0) return false;
    const uint32_t a = SumOf(object->mHistory22C);
    const uint32_t b = SumOf(object->mHistory1A4);
    value = static_cast<int32_t>(b != 0 ? (a * 100u) / b : 0u);   // divwu by 0 is undefined on the Mac; the sums agree, so b is not 0 here
    return true;
}

// The satisfaction variants' number.
bool SatisfactionOf(const CObject* object, int32_t& value) {
    value = object->GetCustomerSatisfaction();
    return true;
}

// The sideshow BestSatisfaction's number: like the satisfaction, after the 1A4 sum has converted to nonzero.
bool SideshowSatisfactionOf(const CObject* object, int32_t& value) {
    if (SumOf(object->mHistory1A4) == 0) return false;
    value = object->GetCustomerSatisfaction();
    return true;
}

// The walk. `id` is cleared first (through the reference); a kept thing writes its id there.
template <class Filter, class Number>
int32_t Query(const CParkAnalyser* analyser, ThingIdRef id, Pick pick, Filter filter, Number number) {
    const CWorldSavvy* savvy = Savvy(analyser);
    bool found = false;
    int32_t kept = 0;
    id.mIndex = 0;
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == kObjectKind) {
            const CObject* object = reinterpret_cast<const CObject*>(thing);
            int32_t value;
            if (filter(object) && object->GetObjectAge() > kMinimumAge && number(object, value)) {
                const bool better = !found || (pick == Pick::kBest ? kept < value : value < kept);
                if (better) {
                    found = true;
                    kept = value;
                    id.mIndex = cur;
                }
            }
        }
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    return found ? kept : 0;
}

}  // namespace

// 0x100eaa24 (476 bytes)
int32_t CParkAnalyser::GetShopWithBestValue(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kBest, OfType{type}, ValueOf);
}

// 0x100eac7c (476 bytes)
int32_t CParkAnalyser::GetShopWithWorstValue(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, OfType{type}, ValueOf);
}

// 0x100eaed4 (348 bytes)
int32_t CParkAnalyser::GetShopWithBestSatisfaction(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kBest, OfType{type}, SatisfactionOf);
}

// 0x100eb0b4 (348 bytes)
int32_t CParkAnalyser::GetShopWithWorstSatisfaction(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, OfType{type}, SatisfactionOf);
}

// 0x100eb294 (520 bytes)
int32_t CParkAnalyser::GetShopWithBestValue(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, AnyShop{}, ValueOf);
}

// 0x100eb504 (520 bytes)
int32_t CParkAnalyser::GetShopWithWorstValue(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, AnyShop{}, ValueOf);
}

// 0x100eb778 (392 bytes)
int32_t CParkAnalyser::GetShopWithBestSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, AnyShop{}, SatisfactionOf);
}

// 0x100eb970 (392 bytes)
int32_t CParkAnalyser::GetShopWithWorstSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, AnyShop{}, SatisfactionOf);
}

// 0x100ebb68 (488 bytes)
int32_t CParkAnalyser::GetSideshowWithBestValue(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, Sideshow{}, ValueOf);
}

// 0x100ebdbc (488 bytes)
int32_t CParkAnalyser::GetSideshowWithWorstValue(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, Sideshow{}, ValueOf);
}

// 0x100ec014 (416 bytes)
int32_t CParkAnalyser::GetSideshowWithBestSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, Sideshow{}, SideshowSatisfactionOf);
}

// 0x100ec228 (488 bytes): the value ratio, not the satisfaction (an original copy-paste of GetSideshowWithWorstValue).
int32_t CParkAnalyser::GetSideshowWithWorstSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, Sideshow{}, ValueOf);
}
