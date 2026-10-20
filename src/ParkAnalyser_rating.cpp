// The park analyser's rating, value and attraction points (main app, analyser.cpp range; unit w159): CParkAnalyser::GetOverallParkRating 0x100ed8cc
// (3,468 bytes), GetParkValue 0x100e5110 (720 bytes) and GetTotalParkAttractionPoints 0x100e5b1c (616 bytes).
//
// GetOverallParkRating is the sum of 11 terms, each a MIN of a count and a cap. The source's MIN macro evaluated its winning argument twice and
// CodeWarrior inlined the first evaluation (the walk of the used things, a copy of NumberOfVisitorsInPark, NumberOfObjectTypeInPark or
// NumberOfStaffType) and CALLED the real function for the second, which it does only when the walk's value is under the cap. Each walk is the
// readers' (src/ParkAnalyser_readers.cpp): GetWorld().GetUsedThingHead(), then per thing GetWorld(), the assertion that entry 0 of the thing table
// is free, the test, GetWorld().GetNextUsedThing(thing). The end ids are halfwords in initialised data, all 0 and never written (0x102a04c6 for the
// visitors', 0x102a04fa for the objects', 0x102a04d8 for the staff's; GetParkValue's object list 0x102a0458, the attraction points' 0x102a045e).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MapCell.h"
#include "ObjectControl.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

constexpr uint16_t kNoThing = 0;
constexpr uint8_t kPersonKind = 1;
constexpr uint8_t kObjectKind = 3;
constexpr int32_t kFirstStaffKind = 4;
constexpr int32_t kLastStaffKind = 8;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242

// The MIN macro of the source, as the listing has it: `first` is the inlined first evaluation, `second` the real call (made only when the first
// is under the cap), the answer is the cap otherwise.
#define TPW_MIN(first, second, cap) ((first) < (cap) ? (second) : (cap))

const CWorldSavvy* Savvy(const CParkAnalyser* analyser) { return reinterpret_cast<const CWorldSavvy*>(analyser); }

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The used-thing walk: `visit(thing)` between the assertion and the call of GetNextUsedThing.
template <class Visit>
void WalkUsedThings(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        visit(thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The object list walk from the world `world` (the head halfword at +0x1da746): `visit(object)`, then the next id (+0x0c). GetWorld is called per
// object only when `savvy` is given (the attraction points read the global world).
template <class Visit>
void WalkObjectList(const CWorldSavvy* savvy, const CWorld& world, Visit visit) {   // savvy null: no GetWorld call per object
    uint16_t cur = world.mF1DA746;
    while (cur != kNoThing) {
        if (savvy != nullptr) savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CObject* const object = static_cast<CObject*>(gThingArray[cur].mThing);
        visit(object);
        cur = object->mF0C;
    }
}

// The inlined copy of NumberOfVisitorsInPark.
int32_t InlineVisitors(const CParkAnalyser* analyser) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        if (thing->mKind == kPersonKind && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) count++;
    });
    return count;
}

// The inlined copy of NumberOfObjectTypeInPark: the objects of that UI type; for rides (type 0) the upgrade byte (+0x4c, read unsigned) must be at
// least `level`.
int32_t InlineObjectTypes(const CParkAnalyser* analyser, int32_t type, int32_t level) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        if (thing->mKind == kObjectKind) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
            if (info->mUIType == type) {
                if (type != 0 || static_cast<int32_t>(object->mF4C) >= level) count++;
            }
        }
    });
    return count;
}

// The inlined copy of NumberOfStaffType: kinds 4 to 8, the kind equal to `model`, not dead.
int32_t InlineStaff(const CParkAnalyser* analyser, CThing::ThingModel model) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        const int32_t kind = thing->mKind;
        if (kind >= kFirstStaffKind && kind <= kLastStaffKind && kind == static_cast<int32_t>(model) && thing->mDead == 0) count++;
    });
    return count;
}

}  // namespace

// The park rating: the 11 terms are computed in the listing's order (the walk of each, then the real call when under the cap) and added at the end.
//  1. the visitors (people inside the park) at most 1000, times 20 / 1000 (signed);  2. the rides (UI type 0) times 3 / 2, at most 20;
//  3. type 1 objects times 2, at most 10;  4. type 2 times 2, at most 10;  5. type 3, at most 10;  6. the rides upgraded to level 2 or more, at most 10;
//  7 to 11. the staff of kinds 5, 4, 6, 7 and 8, at most 4 each.
int32_t CParkAnalyser::GetOverallParkRating() const {
    const int32_t visitorCount = InlineVisitors(this);
    const int32_t visitors = TPW_MIN(visitorCount, NumberOfVisitorsInPark(), 1000);
    const int32_t visitorsTerm = visitors * 20 / 1000;
    const int32_t rides = InlineObjectTypes(this, 0, 0);
    const int32_t ridesTerm = TPW_MIN(rides * 3 / 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 0) * 3 / 2, 20);
    const int32_t type1 = InlineObjectTypes(this, 1, 0);
    const int32_t type1Term = TPW_MIN(type1 * 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(1), 0) * 2, 10);
    const int32_t type2 = InlineObjectTypes(this, 2, 0);
    const int32_t type2Term = TPW_MIN(type2 * 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(2), 0) * 2, 10);
    const int32_t type3 = InlineObjectTypes(this, 3, 0);
    const int32_t type3Term = TPW_MIN(type3, NumberOfObjectTypeInPark(static_cast<UIObjType>(3), 0), 10);
    const int32_t upgraded = InlineObjectTypes(this, 0, 2);
    const int32_t upgradedTerm = TPW_MIN(upgraded, NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 2), 10);
    const int32_t staff5 = InlineStaff(this, static_cast<CThing::ThingModel>(5));
    const int32_t staff5Term = TPW_MIN(staff5, NumberOfStaffType(static_cast<CThing::ThingModel>(5)), 4);
    const int32_t staff4 = InlineStaff(this, static_cast<CThing::ThingModel>(4));
    const int32_t staff4Term = TPW_MIN(staff4, NumberOfStaffType(static_cast<CThing::ThingModel>(4)), 4);
    const int32_t staff6 = InlineStaff(this, static_cast<CThing::ThingModel>(6));
    const int32_t staff6Term = TPW_MIN(staff6, NumberOfStaffType(static_cast<CThing::ThingModel>(6)), 4);
    const int32_t staff7 = InlineStaff(this, static_cast<CThing::ThingModel>(7));
    const int32_t staff7Term = TPW_MIN(staff7, NumberOfStaffType(static_cast<CThing::ThingModel>(7)), 4);
    const int32_t staff8 = InlineStaff(this, static_cast<CThing::ThingModel>(8));
    const int32_t staff8Term = TPW_MIN(staff8, NumberOfStaffType(static_cast<CThing::ThingModel>(8)), 4);
    return visitorsTerm + ridesTerm + type1Term + type2Term + type3Term + upgradedTerm + staff5Term + staff4Term + staff6Term + staff7Term + staff8Term;
}

#undef TPW_MIN

// The park value, cached in mVal1F0E8 for `seconds` seconds (the visitors' cache test: the flag byte is any nonzero value, the seconds compare is
// unsigned). The sum: per map cell (x then y from 127 down to 0; the cell is world + 0x2d8 + (place - 1) * 0x44) without flag 0x40: water 1, path 3,
// queue 5, entrance 1, exit 1, bridge 10, grass 2, track 50; then per object of the object list that is not condemned the sum of the upgrade costs
// of levels 0 to its upgrade byte times the low byte of its mF40 (fctiwz) / 100 (signed, the product wraps).
int32_t CParkAnalyser::GetParkValue(uint32_t seconds) const {
    uint8_t workedOut;
    std::memcpy(&workedOut, &mVal1F0E8.mWorkedOut, 1);
    if (workedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&mVal1F0E8)->SecondsSinceLastUpdate())) {
        uint32_t total = 0;
        const CWorldSavvy* savvy = Savvy(this);
        CWorld& world = const_cast<CWorld&>(savvy->GetWorld());
        for (int32_t x = 127; x >= 0; --x) {
            for (int32_t y = 127; y >= 0; --y) {
                const uint16_t place = static_cast<uint16_t>(x + (y << 7) + 1);
                CMapCell* const cell = &world.mMapCells[place - 1];
                if (cell->IsFlag(0x40) == 0) {
                    if (cell->IsWater() != 0) total += 1;
                    if (cell->IsPath() != 0) total += 3;
                    if (cell->IsQueue() != 0) total += 5;
                    if (cell->IsEntrance() != 0) total += 1;
                    if (cell->IsExit() != 0) total += 1;
                    if (cell->IsBridge() != 0) total += 10;
                    if (cell->IsGrass() != 0) total += 2;
                    if (cell->IsTrack() != 0) total += 50;
                }
            }
        }
        WalkObjectList(savvy, savvy->GetWorld(), [&](CObject* object) {
            int32_t value = 0;
            if (object->IsCondemned() == 0) {
                uint32_t sum = 0;
                for (int32_t i = 0; i <= static_cast<int32_t>(object->mF4C); ++i) {
                    const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
                    sum += static_cast<uint32_t>(info->Value(static_cast<RideBalanceSlot>(kRideSlot_Upgrades_CostOfUpgrade + 16 * i)).mInteger);
                }
                const uint32_t scale = static_cast<uint32_t>(ppc_fctiwz(object->mF40)) & 0xff;
                value = static_cast<int32_t>(sum * scale) / 100;
            }
            total += static_cast<uint32_t>(value);
        });
        const_cast<CParkAnalyserVal<int>&>(mVal1F0E8).Set(static_cast<int32_t>(total));
    }
    return mVal1F0E8.Get();
}

// The attraction points: over the object list from the GLOBAL world's head (never GetWorld), for each rides or shops object (UI type 0 or 2) that is
// available to choose and has a positive count in the park: (the ride's new bonus for its age slot, 0 outside 0 to 2, plus its attraction value)
// divided by the count. A ride record whose track type is 0 counts all the type in the park and takes the age from the days since the type's first
// build (the low word of the 64-bit quotient); any other takes count 1 and the object's age. The age slot is age / the decay time (signed divw; a
// decay time of 0 is undefined on the Mac).
int32_t CParkAnalyser::GetTotalParkAttractionPoints() {
    int32_t total = 0;
    CWorld* const world = gWorld;
    WalkObjectList(nullptr, *world, [&](CObject* object) {
        const uint16_t type = object->mObjectType;
        const int32_t uiType = gRideDatabase.GetRideInfo(ObjectIdOf(type))->mUIType;
        if (uiType != 0 && uiType != 2) return;
        if (object->IsAvailableToChoose() == 0) return;
        if (NumberOfObjectInPark(ObjectIdOf(type)) <= 0) return;
        int32_t count;
        int32_t age;
        if (gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_WhichTrackType).mInteger == 0) {
            count = NumberOfObjectInPark(ObjectIdOf(type));
            ObjectControl* const control = gWorld->GetControlManager()->GetObjectControl(ObjectIdOf(type), 0);
            const TbTimeDiff span = gWorld->mParkTime.GetFunnyTimeSinceGameTurn(control->mFirstBuiltTurn);
            age = static_cast<int32_t>(span.mTicks / 864000000000LL);
        } else {
            count = 1;
            age = object->GetObjectAge();
        }
        const int32_t decay = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_NewAttractionDecayTime).mInteger;
        const int32_t slot = age / decay;
        int32_t points;
        if (slot < 3 && slot >= 0) {
            points = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(static_cast<RideBalanceSlot>(kRideSlot_Attraction_NewBonus + slot)).mInteger;
        } else {
            points = 0;
        }
        const int32_t value = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_AttractionValue).mInteger;
        // the sum and the total wrap as the Mac's 32-bit add does
        const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(points) + static_cast<uint32_t>(value));
        total = static_cast<int32_t>(static_cast<uint32_t>(total) + static_cast<uint32_t>(sum / count));
    });
    return total;
}
