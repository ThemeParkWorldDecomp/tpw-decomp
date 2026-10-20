// The park analyser's queries over the objects and the ride database (main app, analyser.cpp range; unit w151): 26 functions from
// CParkAnalyser::NumberOfToiletsInPark 0x100e8f28 to SetLastOfObjectTypeResearched 0x100ea968.
//
// Three walks, all with the call sequence of the listings:
//  * the used-thing walk of HowBusyIsPark (src/ParkAnalyser_readers.cpp): GetWorld().GetUsedThingHead(), then per thing GetWorld(), the assertion
//    that entry 0 of the thing table is free, the thing from gThingArray[id].mThing, the test, GetWorld().GetNextUsedThing(thing). The end id
//    is a halfword in initialised data (a different one per function), all 0 and never written. An object is a thing of kind 3.
//  * the object list walk (NumberOfBrokenDownRidesInPark, NumberOfWornRidesInPark): from the world's halfword at +0x1da746 (the head of the
//    object list), each object's halfword at +0x0c is the next id, 0 ends. No kind test.
//  * the ride database walk: GetFirstRide, GetNextRide, and per ride the object control of its id, from the CONST GetObjectControl (0x100fa1b8)
//    of the control manager at world + 0x1d943c. A ride record's golden ticket slot (46, +0xc4) is read as a signed word.
#include <cstdint>

#include "Debug.h"
#include "ObjectControl.h"
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
constexpr uint16_t kToiletFlag = 1;
constexpr uint16_t kStaffRoomFlag = 2;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240129 + 0x119 = 0x10240242

const CWorldSavvy* Savvy(const CParkAnalyser* analyser) { return reinterpret_cast<const CWorldSavvy*>(analyser); }

// The used-thing walk: `visit(id, thing)` is called for every thing of the list, between the assertion and the call of GetNextUsedThing.
template <class Visit>
void WalkUsedThings(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        visit(cur, thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The object list walk: `visit(object)` for every object from the world's list head; the next id is read after it.
template <class Visit>
void WalkObjectList(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().mF1DA746;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CObject* const object = static_cast<CObject*>(gThingArray[cur].mThing);
        visit(object);
        cur = object->mF0C;
    }
}

CObject* AsObject(CThing* thing) { return reinterpret_cast<CObject*>(thing); }

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The object control of a ride record: GetWorld() once, then the const GetObjectControl(id, 0) of the manager at world + 0x1d943c.
const ObjectControl* ControlOf(const CParkAnalyser* analyser, const RideInfo* info) {
    const CControlManager* manager =
        reinterpret_cast<const CControlManager*>(reinterpret_cast<uintptr_t>(&Savvy(analyser)->GetWorld()) + 0x1d943c);
    return manager->GetObjectControl(ObjectIdOf(info->mId), 0);
}

// The golden ticket cost slot as a signed word.
int32_t GoldenTicketCost(const RideInfo* info) { return info->Value(kRideSlot_GoldenTicketCost).mInteger; }

// The filth level of a toilet: 100 minus the low byte of the cleanliness float truncated (fctiwz).
int32_t FilthOf(const CObject* object) { return 100 - (ppc_fctiwz(object->mF40) & 0xff); }

// The constant UI type of AllObjectsResearchedAndPresent's skipped types.
constexpr int32_t kUITypeSkipped = 4;

// The research lab of the world (the const twin, 0x1013ec58).
const CResearchLab* LabOf(const CParkAnalyser* analyser) { return static_cast<const CWorld&>(Savvy(analyser)->GetWorld()).GetResearchLab(); }

}  // namespace

// Counts the objects with the toilet flag, hands the count to the cached value (mVal1F0DC) and answers what Get answers afterwards (the
// decompiler shows a void: r3 after Get is the answer).
int32_t CParkAnalyser::NumberOfToiletsInPark() const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) count++;
    });
    const_cast<CParkAnalyserVal<int>&>(mVal1F0DC).Set(count);
    return mVal1F0DC.Get();
}

int32_t CParkAnalyser::NumberOfClosedToiletsInPark() const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0 && AsObject(thing)->IsClosed()) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfStaffRoomsInPark() const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kStaffRoomFlag) != 0) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfClosedStaffRoomsInPark() const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kStaffRoomFlag) != 0 && AsObject(thing)->IsClosed()) count++;
    });
    return count;
}

// The mean filth of the toilets (the signed sum divided by the count), 0 when there is none.
int32_t CParkAnalyser::AvgFilthLevelOfToilets() const {
    int32_t sum = 0;
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) {
            count++;
            sum += FilthOf(AsObject(thing));
        }
    });
    if (count == 0) return 0;
    return sum / count;
}

// The first toilet, or one strictly filthier than the best so far, wins; `filth` is cleared first and written (with the id) only when a toilet was
// found; otherwise the id answered is 0.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CParkAnalyser::FilthiestToilet(int32_t& filth) const {
    uint16_t bestId = 0;
    int32_t best = 0;
    bool found = false;
    filth = 0;
    WalkUsedThings(this, [&](uint16_t id, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) {
            const int32_t level = FilthOf(AsObject(thing));
            if (!found || level > best) {
                best = level;
                found = true;
                bestId = id;
            }
        }
    });
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    if (found) {
        filth = best;
        result.mIndex = bestId;
    } else {
        result.mIndex = 0;
    }
    return result;
}

// The rides (UI type 0) of the object list that are broken down.
int32_t CParkAnalyser::NumberOfBrokenDownRidesInPark() const {
    int32_t count = 0;
    WalkObjectList(this, [&](CObject* object) {
        const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
        if (info->mUIType == 0 && object->IsBrokenDown()) count++;
    });
    return count;
}

// The rides of the object list whose wear byte (the low byte of the truncated mF40) is below `level` (signed), or that are broken down.
int32_t CParkAnalyser::NumberOfWornRidesInPark(int32_t level) const {
    int32_t count = 0;
    WalkObjectList(this, [&](CObject* object) {
        const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
        if (info->mUIType == 0) {
            const int32_t wear = ppc_fctiwz(object->mF40) & 0xff;
            if (wear < level || object->IsBrokenDown()) count++;
        }
    });
    return count;
}

// The objects whose record has that UI type; for rides (type 0) only those whose upgrade byte (+0x4c) is at least `level` (signed).
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind) {
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(AsObject(thing)->mObjectType));
            if (type == info->mUIType) {
                if (type != 0 || static_cast<int32_t>(AsObject(thing)->mF4C) >= level) count++;
            }
        }
    });
    return count;
}

int32_t CParkAnalyser::NumberOfClosedObjectTypeInPark(UIObjType type) const {
    int32_t count = 0;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind) {
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(AsObject(thing)->mObjectType));
            if (type == info->mUIType && AsObject(thing)->IsClosed()) count++;
        }
    });
    return count;
}

// The largest upgrade byte (+0x4c) of the objects of that type, -1 when there is none.
int32_t CParkAnalyser::GetHighestUpgradeLevelForObject(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    int32_t highest = -1;
    WalkUsedThings(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && AsObject(thing)->mObjectType == id.mIndex) {
            const int32_t level = AsObject(thing)->mF4C;
            if (level > highest) highest = level;
        }
    });
    return highest;
}

// The researched ride types; one with a golden ticket cost (slot 46 not 0) counts only when `includeGoldenTickets` is nonzero (its low byte).
int32_t CParkAnalyser::TypesOfRidesAvailable(bool includeGoldenTickets) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == 0) {
            if (ControlOf(this, info)->mAvailable != 0) {
                if (GoldenTicketCost(info) == 0) count++;
                else if (includeGoldenTickets) count++;
            }
        }
    }
    return count;
}

// The ride types with upgrades available (the argument is not read).
int32_t CParkAnalyser::TypesOfUpgradesAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == 0) {
            if (ControlOf(this, info)->mUpgradesAvailable > 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfShopsAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjShop) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfSideshowsAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjSideshow) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfFeaturesAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjFeature) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfRidesLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(0)); }
int32_t CParkAnalyser::TypesOfShopsLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(1)); }
int32_t CParkAnalyser::TypesOfSideshowsLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(2)); }
int32_t CParkAnalyser::TypesOfFeaturesLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(3)); }

// Nothing left to research (category 5, signed above 0 fails) and every record of a UI type other than 4 has one in the park or a golden ticket cost.
bool CParkAnalyser::AllObjectsResearchedAndPresent() const {
    if (LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(5)) > 0) return false;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType != kUITypeSkipped) {
            if (ControlOf(this, info)->mNumberInPark == 0 && GoldenTicketCost(info) == 0) return false;
        }
    }
    return true;
}

// Of the records with a golden ticket cost above 0, the share (in percent, truncated) with at least one in the park; 0 when there are none.
int32_t CParkAnalyser::PercentageOfTotalObjectsInPark() const {
    int32_t total = 0;
    int32_t present = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (GoldenTicketCost(info) > 0) {
            total++;
            if (ControlOf(this, info)->mNumberInPark > 0) present++;
        }
    }
    if (total == 0) return 0;
    return present * 100 / total;
}

// The records of that UI type with one or more in the park; a golden ticket one counts only when `includeGoldenTickets` is set.
int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == type) {
            if (ControlOf(this, info)->mNumberInPark > 0) {
                if (includeGoldenTickets) count++;
                else if (GoldenTicketCost(info) == 0) count++;
            }
        }
    }
    return count;
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    const CControlManager* manager =
        reinterpret_cast<const CControlManager*>(reinterpret_cast<uintptr_t>(&Savvy(this)->GetWorld()) + 0x1d943c);
    return manager->GetObjectControl(id, 0)->mNumberInPark;
}

// The history of that UI type: its value `back` places back, or id 0 when it has none.
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CParkAnalyser::LastOfObjectTypeResearched(UIObjType type, int32_t back) const {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> out;
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> result;
    if (mLastOfObjectType[type].GetValue(out, back)) result.mIndex = out.mIndex;
    else result.mIndex = 0;
    return result;
}

void CParkAnalyser::SetLastOfObjectTypeResearched(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    mLastOfObjectType[type].SetAtEnd(id);
}
