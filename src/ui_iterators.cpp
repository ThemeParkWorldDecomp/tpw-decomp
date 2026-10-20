// UI thing iterators, main app ui_iterators.cpp (unit w227). Addresses and sizes are in ui_iterators.h.
#include "ui_iterators.h"

#include "Debug.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"

// The Mac's file initialiser stores the second word first. Neither word has a known reader.
uint32_t gUIIterFileWord10396A4C = 0x10000;
uint32_t gUIIterFileWord10396A50 = 0;

namespace {
constexpr uint16_t kNoThing = 0;
constexpr uintptr_t kStaffNextOffset = 0x20c;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"; // 0x1024f650

CThing* FetchThing(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    return static_cast<CThing*>(gThingArray[id].mThing);
}

uint16_t StaffNext(const CThing* thing) {
    return *reinterpret_cast<const uint16_t*>(reinterpret_cast<uintptr_t>(thing) + kStaffNextOffset);
}

uint16_t StaffHead(UIIteratorTypes type) {
    switch (type) {
        case kUIIterHandymen: return gGameWorld->mF1DA73C;
        case kUIIterMechanics: return gGameWorld->mF1DA73E;
        case kUIIterGuards: return gGameWorld->mF1DA744;
        case kUIIterEntertainers: return gGameWorld->mF1DA740;
        default: return gGameWorld->mF1DA742;
    }
}

bool IsStaff(const CThing* thing) {
    const uint8_t kind = thing->mKind;
    return kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7;
}

bool IsObjectType(CThing* thing, UIObjType type) {
    CObject* const object = reinterpret_cast<CObject*>(thing);
    return thing->mKind == 3 && object->UpgradesWhat().mIndex == kNoThing && object->GetDBInfo()->mUIType == type;
}

bool IsUpgrade(CThing* thing) {
    return thing->mKind == 3 && reinterpret_cast<CObject*>(thing)->UpgradesWhat().mIndex != kNoThing;
}

bool IsNonTrackUpgrade(CThing* thing) {
    CObject* const object = reinterpret_cast<CObject*>(thing);
    return thing->mKind == 3 && object->UpgradesWhat().mIndex != kNoThing && object->GetDBInfo()->Value(kRideSlot_UpgradeType).mInteger == 2;
}

bool IsBog(CThing* thing) {
    return IsObjectType(thing, kUIObjFeature) && (reinterpret_cast<CObject*>(thing)->mFlags & 1);
}

bool IsPlainFeature(CThing* thing) {
    return IsObjectType(thing, kUIObjFeature) && !(reinterpret_cast<CObject*>(thing)->mFlags & 1) && !(reinterpret_cast<CObject*>(thing)->mFlags & 2);
}

bool IsStaffRoom(CThing* thing) {
    return thing->mKind == 3 && (reinterpret_cast<CObject*>(thing)->mFlags & 2);
}

// The search still advances once after a hit. A null backward start first finds the last used thing.
template <class Predicate>
CThing* FindThing(CThing* thing, bool previous, Predicate predicate) {
    uint16_t cur;
    if (!previous) {
        cur = thing == nullptr ? gGameWorld->GetUsedThingHead().mIndex : gGameWorld->GetNextUsedThing(thing).mIndex;
    } else if (thing != nullptr) {
        cur = gGameWorld->GetPrevUsedThing(thing).mIndex;
    } else {
        CThing* last = nullptr;
        cur = gGameWorld->GetUsedThingHead().mIndex;
        while (cur != kNoThing) {
            last = FetchThing(cur);
            cur = gGameWorld->GetNextUsedThing(last).mIndex;
        }
        // The original calls GetThingId even when the used list is empty and last is null.
        cur = last->GetThingId().mIndex;
    }
    CThing* found = nullptr;
    while (cur != kNoThing && found == nullptr) {
        CThing* const current = FetchThing(cur);
        if (predicate(current)) {
            found = current;
        }
        cur = previous ? gGameWorld->GetPrevUsedThing(current).mIndex : gGameWorld->GetNextUsedThing(current).mIndex;
    }
    return found;
}

bool MatchesList(UIIteratorTypes type, CThing* thing) {
    switch (type) {
        case kUIIterStaff: return IsStaff(thing);
        case kUIIterVisitors: return thing->mKind == 1;
        case kUIIterRides: return IsObjectType(thing, kUIObjRide);
        case kUIIterSideshows: return IsObjectType(thing, kUIObjSideshow);
        case kUIIterShops: return IsObjectType(thing, kUIObjShop);
        case kUIIterFeatures: return IsObjectType(thing, kUIObjFeature);
        case kUIIterPlainFeatures: return IsPlainFeature(thing);
        case kUIIterBogs: return IsBog(thing);
        case kUIIterUpgrades: return IsUpgrade(thing);
        case kUIIterNonTrackUpgrades: return IsNonTrackUpgrade(thing);
        // List's staff-room predicate is stronger than the standalone searches'.
        case kUIIterStaffRooms: return IsObjectType(thing, kUIObjFeature) && (reinterpret_cast<CObject*>(thing)->mFlags & 2);
        default: return false;
    }
}
} // namespace

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    int32_t total = 0;
    for (uint32_t bit = 1; bit <= 0x8000; bit <<= 1) {
        if (!(static_cast<uint32_t>(mask) & bit)) {
            continue;
        }
        const UIIteratorTypes type = static_cast<UIIteratorTypes>(bit);
        int32_t count = 0;
        const bool staffList = bit >= 2 && bit <= 0x20;
        uint16_t cur = staffList ? StaffHead(type) : gGameWorld->GetUsedThingHead().mIndex;
        while (cur != kNoThing) {
            CThing* const thing = FetchThing(cur);
            if (staffList || MatchesList(type, thing)) {
                fn(thing);
                ++count;
            }
            // The callback may replace both the staff link and gGameWorld.
            cur = staffList ? StaffNext(thing) : gGameWorld->GetNextUsedThing(thing).mIndex;
        }
        total += count;
    }
    return total;
}

CThing* NextObjectOfType(UIObjType type, CThing* thing) {
    return FindThing(thing, false, [type](CThing* t) { return IsObjectType(t, type); });
}
CThing* NextUpgrade(CThing* thing) { return FindThing(thing, false, IsUpgrade); }
CThing* NextNonTrackUpgrade(CThing* thing) { return FindThing(thing, false, IsNonTrackUpgrade); }
CThing* NextBog(CThing* thing) { return FindThing(thing, false, IsBog); }
CThing* NextFeatureNotBogOrStaffRoom(CThing* thing) { return FindThing(thing, false, IsPlainFeature); }
CThing* NextStaffRoom(CThing* thing) { return FindThing(thing, false, IsStaffRoom); }

CThing* UI_IterateThingNext(UIIteratorTypes type, CThing* thing) {
    switch (type) {
        case kUIIterStaff: return FindThing(thing, false, IsStaff);
        case kUIIterHandymen:
        case kUIIterMechanics:
        case kUIIterGuards:
        case kUIIterEntertainers:
        case kUIIterResearchers: {
            const uint16_t id = thing == nullptr ? StaffHead(type) : StaffNext(thing);
            return id == kNoThing ? nullptr : FetchThing(id);
        }
        case kUIIterVisitors: return FindThing(thing, false, [](CThing* t) { return t->mKind == 1; });
        case kUIIterRides: return NextObjectOfType(kUIObjRide, thing);
        case kUIIterSideshows: return NextObjectOfType(kUIObjSideshow, thing);
        case kUIIterShops: return NextObjectOfType(kUIObjShop, thing);
        case kUIIterFeatures: return NextObjectOfType(kUIObjFeature, thing);
        case kUIIterPlainFeatures: return NextFeatureNotBogOrStaffRoom(thing);
        case kUIIterBogs: return NextBog(thing);
        case kUIIterUpgrades: return NextUpgrade(thing);
        case kUIIterNonTrackUpgrades: return NextNonTrackUpgrade(thing);
        case kUIIterStaffRooms: return NextStaffRoom(thing);
        default: return nullptr;
    }
}

CThing* PreviousObjectOfType(UIObjType type, CThing* thing) {
    return FindThing(thing, true, [type](CThing* t) { return IsObjectType(t, type); });
}
CThing* PreviousUpgrade(CThing* thing) { return FindThing(thing, true, IsUpgrade); }
CThing* PreviousNonTrackUpgrade(CThing* thing) { return FindThing(thing, true, IsNonTrackUpgrade); }
CThing* PreviousBog(CThing* thing) { return FindThing(thing, true, IsBog); }
CThing* PreviousFeatureNotBogOrStaffRoom(CThing* thing) { return FindThing(thing, true, IsPlainFeature); }
CThing* PreviousStaffRoom(CThing* thing) { return FindThing(thing, true, IsStaffRoom); }

CThing* UI_IterateThingPrevious(UIIteratorTypes type, CThing* thing) {
    switch (type) {
        case kUIIterStaff: return FindThing(thing, true, IsStaff);
        // The five staff selections share the visitor label in the original switch.
        case kUIIterHandymen:
        case kUIIterMechanics:
        case kUIIterGuards:
        case kUIIterEntertainers:
        case kUIIterResearchers:
        case kUIIterVisitors: return FindThing(thing, true, [](CThing* t) { return t->mKind == 1; });
        case kUIIterRides: return PreviousObjectOfType(kUIObjRide, thing);
        case kUIIterSideshows: return PreviousObjectOfType(kUIObjSideshow, thing);
        case kUIIterShops: return PreviousObjectOfType(kUIObjShop, thing);
        case kUIIterFeatures: return PreviousObjectOfType(kUIObjFeature, thing);
        case kUIIterPlainFeatures: return PreviousFeatureNotBogOrStaffRoom(thing);
        case kUIIterBogs: return PreviousBog(thing);
        case kUIIterUpgrades: return PreviousUpgrade(thing);
        case kUIIterNonTrackUpgrades: return PreviousNonTrackUpgrade(thing);
        case kUIIterStaffRooms: return PreviousStaffRoom(thing);
        default: return nullptr;
    }
}

CThing* UI_WrapIterateThingNext(UIIteratorTypes type, CThing* thing) {
    CThing* result = UI_IterateThingNext(type, thing);
    if (result == nullptr) {
        result = UI_IterateThingNext(type, nullptr);
    }
    return result;
}

CThing* UI_WrapIterateThingPrevious(UIIteratorTypes type, CThing* thing) {
    CThing* result = UI_IterateThingPrevious(type, thing);
    if (result == nullptr) {
        result = UI_IterateThingPrevious(type, nullptr);
    }
    return result;
}
