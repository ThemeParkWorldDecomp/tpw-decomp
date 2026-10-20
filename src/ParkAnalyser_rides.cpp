// The park analyser's ride superlatives (main app, analyser.cpp range; unit w153): CParkAnalyser::GetMostExcitingGokart 0x100ec484 (376 bytes),
// GetGokartWithMostCrossRoads 0x100ec668 (384), GetLongestTrackRide 0x100ec858 (424) and its int overload 0x100eca98 (384), GetCoasterWithMostLoops
// 0x100ecc80 (376), GetHighestCoaster 0x100ece94 (408), GetAverageToiletQueueLenght 0x100ed0c0 (316), FindRideWithUnconnectedQueue 0x100ed240
// (284), GetAverageExcitementLevelOfAllRides 0x100ed3a0 (400), GetScariestRide 0x100ed57c (344) and GetRidesInPark 0x100e558c (344).
//
// Every one walks the used list of things as HowBusyIsPark does (src/ParkAnalyser_readers.cpp): per thing the world is asked, entry 0 of the thing
// table is asserted free (the text at 0x10240242), the thing is read from gThingArray[id].mThing, the thing is tested and the world gives the
// next. The id that ends a walk is a halfword of initialised data (0x102a0516 and the ones after it, one per function), all 0 and never written.
// A ride is an object (kind byte 3) whose ride record (CRideDatabase::GetRideInfo of the halfword at +0x0e) has mUIType 0.
#include <cstdint>

#include "Debug.h"
#include "LLCellSystem.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "TPEngine.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectId;

constexpr uint16_t kNoThing = 0;     // the end ids and the "any" ids, all data halfwords of 0
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242
constexpr int32_t kWhichTrackTypeGokart = 1;   // the record's WhichTrackType of a go-kart track

// Walks the used list through the analyser as a CWorldSavvy; `body(thing, id)` is called for every thing, in the listings' order.
template <class Body>
void WalkThings(const CParkAnalyser* analyser, Body body) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        body(thing, cur);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The thing is an object whose ride record says it is a ride (mUIType 0).
bool IsRide(CThing* thing) {
    if (thing->mKind != 3) return false;
    CObject* const object = reinterpret_cast<CObject*>(thing);
    return gRideDatabase.GetRideInfo(ObjectId{object->mObjectType})->mUIType == 0;
}

bool IsGokart(CObject* object) {
    return object->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger == kWhichTrackTypeGokart;
}

// The track sections of the ride: all four outs NULL.
int32_t TrackSections(CObject* object) {
    return BMP_CountTrackSections(object->mF24, nullptr, nullptr, nullptr, nullptr);
}

}  // namespace

// The go-kart ride with the highest excitement level; `id` is cleared first and takes the id of each better ride. A tie keeps the first.
uint8_t CParkAnalyser::GetMostExcitingGokart(ThingId& id) const {
    bool found = false;
    uint8_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (!IsGokart(object)) return;
        const uint8_t level = object->GetExcitementLevel(false);
        if (!found || best < level) {
            best = level;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The go-kart ride with the most cross roads: the third out of BMP_CountTrackSections, a signed compare.
int32_t CParkAnalyser::GetGokartWithMostCrossRoads(ThingId& id) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (!IsGokart(object)) return;
        int32_t crossRoads;   // the stack word the third out points at (unset if the callee leaves it, as on the Mac)
        BMP_CountTrackSections(object->mF24, nullptr, nullptr, &crossRoads, nullptr);
        if (!found || best < crossRoads) {
            best = crossRoads;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The track ride (IsTrackRide nonzero) of object type `type` (0, the halfword at 0x102a051c, means any) with the most track sections (signed).
int32_t CParkAnalyser::GetLongestTrackRide(ThingId& id, ObjectId type) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsTrackRide() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        const int32_t count = TrackSections(object);
        if (!found || best < count) {
            best = count;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The ride whose record's WhichTrackType is `track` (signed word compare) with the most track sections.
int32_t CParkAnalyser::GetLongestTrackRide(ThingId& id, int32_t track) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (track != object->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger) return;
        const int32_t count = TrackSections(object);
        if (!found || best < count) {
            best = count;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The coaster (the ride record is not asked) of object type `type` (0 means any) with the most loops, the fourth out of GetCoasterTrackStats (unsigned).
uint32_t CParkAnalyser::GetCoasterWithMostLoops(ThingId& id, ObjectId type) const {
    bool found = false;
    uint32_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsCoaster() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        float a, height, c;
        uint32_t loops;
        GetCoasterTrackStats(object, &a, &height, &c, &loops);
        if (!found || best < loops) {
            best = loops;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The coaster of object type `type` with the greatest second track statistic (the height). The best so far is a uint32_t; it is compared as a float
// (the exact double of the unsigned word, rounded to single) with the new height, and the new best is the height converted back by CodeWarrior's
// double to unsigned helper. A NaN height never wins a compare (the listing's ble is taken for unordered) but is taken when nothing was found yet.
uint32_t CParkAnalyser::GetHighestCoaster(ThingId& id, ObjectId type) const {
    bool found = false;
    uint32_t best = 0;
    id.mIndex = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsCoaster() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        float a, height, c;
        uint32_t loops;
        GetCoasterTrackStats(object, &a, &height, &c, &loops);
        if (!found || static_cast<float>(best) < height) {
            found = true;
            best = cw_cvt_fp2unsigned(height);
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The mean queue length of the toilets (objects with flag 1): the sum over the count, both as floats; 0.0f when there are none.
float CParkAnalyser::GetAverageToiletQueueLenght() const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing, uint16_t) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if ((object->mFlags & 1) == 0) return;
        count++;
        sum += static_cast<uint32_t>(object->GetNumPeopleInQueue());
    });
    if (count == 0) return 0.0f;
    return static_cast<float>(static_cast<int32_t>(sum)) / static_cast<float>(count);
}

// The first object with flags 4 and 8 whose queue is not connected, answered when `any` or when it is not the cell system's current object. This
// walk asks the global world (gWorld) directly, not CWorldSavvy::GetWorld, and asserts before each thing.
ThingId CParkAnalyser::FindRideWithUnconnectedQueue(bool any) {
    uint16_t cur = gWorld->GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == 3) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            if ((object->mFlags & 4) != 0 && (object->mFlags & 8) != 0 && object->IsBOQConnected() == 0) {
                if (any) return ThingId{cur};
                if (LLCM_GetCurrentObject() != object) return ThingId{cur};
            }
        }
        cur = gWorld->GetNextUsedThing(thing).mIndex;
    }
    return ThingId{0};
}

// The mean excitement level of the rides (integer division, signed), cached in mVal1F0F4: the walk is skipped when the Val has a value and
// SecondsSinceLastUpdate is not above `seconds` (unsigned compare).
int32_t CParkAnalyser::GetAverageExcitementLevelOfAllRides(uint32_t seconds) const {
    CParkAnalyserVal<int>& val = const_cast<CParkAnalyserVal<int>&>(mVal1F0F4);
    if (val.mWorkedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        int32_t sum = 0;
        int32_t count = 0;
        WalkThings(this, [&](CThing* thing, uint16_t) {
            if (!IsRide(thing)) return;
            CObject* const object = reinterpret_cast<CObject*>(thing);
            count++;
            sum += object->GetExcitementLevel(false);
        });
        if (count > 0) val.Set(sum / count);
        else val.Set(0);
    }
    return val.Get();
}

// The ride with the highest excitement level (unsigned byte): no found flag, the best id (0 at the start, 0 meaning none) is written through `id`
// once, after the walk. The first ride is taken because the best id is still 0.
uint8_t CParkAnalyser::GetScariestRide(ThingId& id) const {
    uint16_t bestId = 0;
    uint8_t best = 0;
    WalkThings(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        const uint8_t level = object->GetExcitementLevel(false);
        if (bestId == kNoThing || best < level) {
            best = level;
            bestId = cur;
        }
    });
    id.mIndex = bestId;
    return best;
}

// The number of rides, cached in mVal1F0C4 like the average above.
int32_t CParkAnalyser::GetRidesInPark(uint32_t seconds) const {
    CParkAnalyserVal<int>& val = const_cast<CParkAnalyserVal<int>&>(mVal1F0C4);
    if (val.mWorkedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        int32_t count = 0;
        WalkThings(this, [&](CThing* thing, uint16_t) {
            if (IsRide(thing)) count++;
        });
        val.Set(count);
    }
    return val.Get();
}
